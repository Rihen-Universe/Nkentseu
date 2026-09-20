// =============================================================================
// NKDesignIABanc — LE JEU D'EPREUVE DE L'IA DE DESIGN : trois taux, pas un avis.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'IL MESURE, ET POURQUOI EN TROIS NIVEAUX SEPARES
// =============================================================================
//  « L'IA repond » ne veut rien dire. Le critere est « LE DOCUMENT S'OUVRE », et
//  il se decompose en trois etages qui echouent pour des raisons DIFFERENTES :
//
//    N1  le modele rend quelque chose   -> code 0 et sortie non vide
//    N2  ce quelque chose est un document -> `ValidateReply` : en-tete `nkuidoc`,
//        structure chargeable, composants du REGISTRE, et REJEU identique
//    N3  le document s'ouvre            -> greffe par la meme porte que la main,
//        mise en page, puis ecriture `.nkgui` (le pont existe depuis le 14/09)
//
//  Les melanger donnerait un taux unique qui ne dit pas quoi reparer. Un modele
//  qui parle et n'ecrit pas le format, un modele qui ecrit le format et invente
//  un composant, un document juste que le pont ne sait pas traduire : trois
//  maladies, trois remedes.
//
//  ⚠️ LE MONTAGE N'EST PAS FAIT ICI, ET C'EST VOULU. Le `.nkgui` produit est
//     donne a `NKGuiMonteTest --monter=<fichier>`, un AUTRE binaire, qui le lit,
//     le valide et le MONTE en widgets reels. Deux instruments sans code commun :
//     si les deux s'accordent, ce n'est pas parce qu'ils partagent un bogue.
//
// =============================================================================
//  CE QU'IL NE FAIT PAS
// =============================================================================
//  - il n'ouvre AUCUNE fenetre et ne touche pas au GPU : le modele vit dans SON
//    processus (contrat a deux trous `{invite}` / `{sortie}`), et ce banc ne fait
//    que l'appeler ;
//  - il ne juge pas la BEAUTE. Le rejeu verifie la fidelite, pas le gout. Un
//    ecran bien forme et laid passe les trois niveaux ;
//  - il n'ecrit rien dans le document de travail de Rodolf : chaque demande part
//    d'un document NEUF.
//
// ⚠️ LE CATALOGUE EST PEUPLE ICI PAR LES MEMES TROIS SOURCES que
//    `PanneauToile::PeuplerRegistre` (Panels.h) : la table des basiques, le
//    navigateur de contenu, l'arbre. C'est une SECONDE ECRITURE de la meme
//    regle, et je le declare : la condition de reouverture est « le jour ou un
//    composant apparait dans l'un et pas dans l'autre ». La porte propre serait
//    que `PeuplerRegistre` descende hors d'un en-tete de panneaux ; ce n'est pas
//    mon territoire aujourd'hui, et un banc qui n'a pas le meme catalogue que
//    l'application mesurerait une autre application.
// =============================================================================

// ⚠️ `using namespace nkentseu;` AVANT LES EN-TETES DE NKUIDesign, ET CE N'EST PAS
//    UN CONFORT : `NkDocStringPool.h` (et, a sa suite, `Document.h`) ecrivent
//    `NkString` et `nk_size` SANS QUALIFICATION a l'interieur de `namespace
//    nkuidesign`. Dans `NKUIDesign/main.cpp` cela compile parce qu'un en-tete
//    inclus plus tot a deja deverse `using namespace nkentseu` au niveau global.
//    Ici, premier consommateur a ne pas l'avoir par hasard, la construction s'est
//    arretee sur neuf erreurs -- exactement « un en-tete partage inclut ce qu'il
//    utilise », mesure une fois de plus. Je le contourne au lieu de refactorer les
//    en-tetes d'un autre chantier ; CONDITION DE RETRAIT de ces deux lignes : le
//    jour ou `NkDocStringPool.h` qualifie ses types.
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
using namespace nkentseu;

#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"
#include "Recolte.h" // la RECOLTE : garder chaque paire, surtout les echecs
#include "NKUIDesign/ComposantsBase.h"
#include "NKUIDesign/DesignAI.h"
#include "NKUIDesign/DesignChat.h" // la SPECIFICATION, batie par SON ecrivain
#include "NKUIDesign/Document.h"
#include "NKUIDesign/Layout.h"
#include "NKUIDesign/NkGuiEcrire.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkuidesign;

namespace {

	// ── OUTILS DE CHEMIN ET DE CHAINE (zero-STL) ─────────────────────────────
	void Joindre(char *dst, usize cap, const char *a, const char *b) {
		usize n = 0;
		for (; a && a[n] && n + 1 < cap; ++n)
			dst[n] = a[n];
		for (usize i = 0; b && b[i] && n + 1 < cap; ++i, ++n)
			dst[n] = b[i];
		dst[n] = 0;
	}

	bool CommencePar(const char *s, const char *p) {
		usize i = 0;
		for (; p[i]; ++i)
			if (s[i] != p[i])
				return false;
		return true;
	}

	/// Le registre, peuple comme l'application le peuple. Voir l'avertissement
	/// de l'en-tete : deux ecritures de la meme regle, declarees comme telles.
	void PeuplerCatalogue() {
		uint32 nbBase = 0;
		(void)basiques::NkTableBasiques(nbBase);
		for (uint32 i = 0; i < nbBase; ++i)
			nkentseu::editorkit::NkComponentRegistry::Register(basiques::NkDeclBasique(i));
		nkentseu::editorkit::NkComponentRegistry::Register(nkentseu::editorkit::NkContentBrowserDecl());
		nkentseu::editorkit::NkComponentRegistry::Register(nkentseu::editorkit::NkTreeViewDecl());
	}

	struct Ligne {
			char id[16] = {0};
			char texte[512] = {0};
	};

	/// Lit `demandes.txt` : les lignes vides et celles qui commencent par '#'
	/// sont ignorees ; le reste s'ecrit « id | phrase ».
	uint32 LireDemandes(const char *chemin, Ligne *out, uint32 cap) {
		const NkString txt = NkFile::ReadAllText(NkPath(chemin));
		if (txt.Empty())
			return 0;
		uint32 nb = 0;
		const char *p = txt.CStr();
		while (*p && nb < cap) {
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			// La ligne brute, sans le retour chariot de Windows.
			char brut[640];
			usize n = 0;
			for (const char *q = p; q < fin && n + 1 < sizeof(brut); ++q)
				if (*q != '\r')
					brut[n++] = *q;
			brut[n] = 0;
			p = *fin ? fin + 1 : fin;
			if (n == 0 || brut[0] == '#')
				continue;
			// couper sur la barre verticale
			char *bar = nullptr;
			for (usize i = 0; i < n; ++i)
				if (brut[i] == '|') {
					bar = brut + i;
					break;
				}
			if (!bar)
				continue;
			*bar = 0;
			// l'identifiant : sans espaces
			usize k = 0;
			for (usize i = 0; brut[i] && k + 1 < sizeof(out[nb].id); ++i)
				if (brut[i] != ' ' && brut[i] != '\t')
					out[nb].id[k++] = brut[i];
			out[nb].id[k] = 0;
			// la phrase : sans l'espace de tete
			const char *ph = bar + 1;
			while (*ph == ' ')
				++ph;
			usize m = 0;
			for (; ph[m] && m + 1 < sizeof(out[nb].texte); ++m)
				out[nb].texte[m] = ph[m];
			out[nb].texte[m] = 0;
			if (out[nb].id[0] && out[nb].texte[0])
				++nb;
		}
		return nb;
	}

	/// Un dorsal de banc : il rend un texte FIXE, sans modele et sans processus.
	/// C'est LE ZERO du banc — il prouve que les trois niveaux savent dire OUI
	/// quand la reponse est bonne, donc qu'un 0/12 mesure le modele et non le
	/// banc. Sans lui, un banc qui refuse tout serait indiscernable d'un banc
	/// qui marche face a un modele faible.
	class DorsalTemoin final : public NkIDesignBackend {
		public:
			bool Complete(const NkDesignRequest &, NkDesignReply &out) override {
				out.text = NkString("nkuidoc 1\n"
									"titre = Temoin\n"
									"noeud 0\n"
									"  libelle = Ecran\n"
									"  agencement = column\n"
									"  ecart = 8\n"
									"  marge = 12\n"
									"  enfants = 1 2\n"
									"noeud 1\n"
									"  libelle = Titre\n"
									"  composant = etiquette\n"
									"noeud 2\n"
									"  libelle = Valider\n"
									"  composant = bouton\n");
				out.success = true;
				return true;
			}
			bool IsAvailable() const override {
				return true;
			}
			const char *Name() const override {
				return "temoin";
			}
	};

	// ═══════════════════════════════════════════════════════════════════
	//  LE CONTRAT D'OUTIL -- ECRIT, VERSIONNE, ET ENGENDRE
	// ═══════════════════════════════════════════════════════════════════
	// Arbitrage de Rodolf, 17/09 au soir : « on peut choisir le modele a
	// utiliser, local ou distant ; le plus important est la PERFORMANCE DES
	// OUTILS qui sont associes au modele. » Consequence directe (cap §6.5) :
	// **le contrat d'outil est ecrit et versionne** -- ce qu'il accepte, ce
	// qu'il connait, ce qu'il refuse et avec quel motif. C'est lui qu'on donne
	// a un modele distant comme a Ilyana ; un contrat qui ne vit que dans la
	// chaine de l'invite n'est partageable avec personne.
	//
	// ⚠️ IL EST ENGENDRE, JAMAIS REDIGE. Chaque section sort de la source qui
	//    fait foi a l'execution : le REGISTRE pour les composants, les memes
	//    fonctions de nom que l'invite pour la grammaire, l'enumeration des
	//    verdicts pour les refus. Un contrat recopie a la main derive au
	//    premier ajout, et il derive EN SILENCE -- c'est-a-dire qu'il promet a
	//    un modele distant des choses que l'application ne sait plus faire.
	//
	// ⚠️ ET IL PORTE SA PROPRE GARDE. `--verifier-contrat=<f>` compare le
	//    fichier versionne a ce que le binaire engendre AUJOURD'HUI, et rend 1
	//    s'ils different. Un fichier engendre puis commite sans garde est une
	//    copie qui se perime en silence, et le depot paie deja ce defaut
	//    ailleurs.
	void BatirContrat(NkString &out) {
		out = NkString("# CONTRAT D'OUTIL -- NKUIDesign, texte vers interface\n");
		out.Append("#\n");
		out.Append("# AUTEUR : TEUGUIA TADJUIDJE Rodolf S\xC3\xA9""deris \xE2\x80\x94 Rihen\n");
		out.Append("#\n");
		out.Append("# ENGENDRE par `NKDesignIABanc --contrat=<fichier>`.\n");
		out.Append("# NE PAS EDITER A LA MAIN : `--verifier-contrat=<fichier>` rougit si ce\n");
		out.Append("# fichier ne dit plus ce que le binaire fait.\n\n");

		out.Append("## 1. CE QUE L'OUTIL ACCEPTE -- la grammaire du document\n\n");
		NkString invite;
		NkDesignAI::BuildPrompt("<la demande de l'utilisateur, en francais>", invite);
		out.Append(invite);
		// ⚠️ LE CONTRAT DOIT DIRE OU ARRIVE LA DEMANDE, sinon il ment par
		//    omission. Depuis le 20/09 elle n'est plus en tete de l'invite : elle
		//    est RAPPELEE EN DERNIER, apres le catalogue et le document courant
		//    (mesure : sur un document de 42 noeuds, le document occupait 84 % de
		//    l'invite et le modele y repondait au lieu de la demande).
		//    Un modele distant qui ne voit QUE ce fichier doit le savoir.
		out.Append("\nORDRE DE L'INVITE : ce format d'abord, puis le catalogue, puis le\n");
		out.Append("document courant s'il y en a un, et EN DERNIER la demande, sous\n");
		out.Append("`--- ce que je te demande maintenant ---`. C'est a CELLE-LA qu'on\n");
		out.Append("repond ; le document courant est un contexte, pas une question.\n");

		out.Append("\n## 2. CE QUE L'OUTIL CONNAIT -- le catalogue des composants\n");
		out.Append("# Boucle sur le registre. Un composant absent d'ici est REFUSE, jamais\n");
		out.Append("# approche par un voisin.\n\n");
		NkString cat;
		NkDesignAI::BuildCatalog(cat);
		out.Append(cat);

		out.Append("\n## 3. CE QUE L'OUTIL REFUSE, ET AVEC QUEL MOTIF\n");
		out.Append("# Un refus nomme est un OUTIL, pas une panne : un modele invente, c'est le\n");
		out.Append("# cas normal. Chacun de ces motifs laisse le document INTACT.\n\n");
		for (uint8 v = 0; v < (uint8)NkAIVerdict::Count; ++v) {
			const NkAIVerdict vv = (NkAIVerdict)v;
			if (vv == NkAIVerdict::Acceptee) {
				continue;
			}
			out.Append("refus ");
			out.Append(NkAIVerdictName(vv));
			out.Append('\n');
		}

		out.Append("\n## 4. CE QUE L'OUTIL GARANTIT\n\n");
		out.Append("garantie le document n'est touche qu'apres validation ET rejeu identique\n");
		out.Append("garantie toute pose se retire d'un geste, document identique au bit\n");
		out.Append("garantie une proposition ne touche pas le document avant sa validation\n");
		out.Append("garantie chaque noeud pose porte sa provenance (auteur ia, origine nommee)\n");

		out.Append("\n## 5. CE QUE L'OUTIL NE SAIT PAS FAIRE -- mesure, pas suppose\n\n");
		out.Append("limite il ne recoit AUCUNE image : la requete porte trois champs, tous du texte\n");
		out.Append("limite il ne sait que GREFFER un sous-arbre : ni renommer, ni deplacer, ni\n");
		out.Append("       changer une propriete d'un noeud existant\n");
		out.Append("limite deux composants du catalogue n'ont pas d'equivalent dans le vocabulaire\n");
		out.Append("       du fichier d'interface : ils sont COMPTES et NOMMES, jamais devines\n");
		// ⚠️ CETTE LIMITE A ETE LEVEE LE 17/09 AU SOIR, ET LE CONTRAT LE DIT PLUTOT
		//    QUE DE SE TAIRE. `Scroll` se monte ; un role inconnu ne perd plus son
		//    sous-arbre. Il RESTE 16 roles du format que le monteur ignore : les
		//    annoncer est la seule facon de ne pas promettre a un modele distant ce
		//    que l'application ne sait pas honorer.
		out.Append("limite 16 roles du vocabulaire du format ne sont pas montes (ColorField,\n");
		out.Append("       Column, ContextMenu, Drag, Flow, Grid, ImageButton, Menu, MenuBar,\n");
		out.Append("       MenuItem, NumberField, RadioGroup, Row, Stack, Switch, Table). Leur\n");
		out.Append("       CONTENU n'est plus perdu depuis le 17/09 -- il monte dans le flux du\n");
		out.Append("       parent -- mais le conteneur est compte inconnu et le document REFUSE\n");
		out.Append("       (mesure : 2 montes -> 6 et REFUSE -> MONTE pour Scroll, desormais monte)\n");
		// ⚠️ LIMITE REDUITE LE 17/09 AU SOIR, PAS LEVEE. L'apparence AU REPOS se
		//    peint sur les quatre roles qui ont une surface ; le reste est compte.
		//    Dire « il peint l'apparence » tout court serait une publicite.
		out.Append("limite le monteur ne peint l'apparence AU REPOS que sur Button, RepeatButton,\n");
		out.Append("       Panel et Window (fond) et Text (encre). Un fill sur un role sans surface,\n");
		out.Append("       radius, font, shadow et stroke sont COMPTES non peints, jamais devines\n");
		out.Append("limite aucun etat d'apparence autre que le repos n'est applique : ce monteur\n");
		out.Append("       monte l'etat au repos, il n'a ni souris ni focus\n");
		// ⚠️ LE `Spacer` FLEXIBLE : ce que le document VEUT DIRE et qu'on ne rend
		//    pas. Il n'invente plus 120 px depuis le 17/09, mais il ne pousse pas
		//    davantage -- et un modele qui ecrit `Spacer {}` doit le savoir.
		out.Append("limite un Spacer sans size, dans une rangee, veut dire prends la place qui\n");
		out.Append("       reste et n'est PAS honore : pousser le voisin jusqu'au bord demanderait\n");
		out.Append("       deux passes. Il vaut zero (le voisin reste dedans) et la demande est\n");
		out.Append("       COMPTEE. Avant le 17/09 il valait 120 px inventes et poussait le voisin\n");
		out.Append("       HORS du panneau, montage vert\n");
		out.Append("mesure  un compteur de DEBORDEMENT existe depuis le 17/09 : il nomme tout widget\n");
		out.Append("       qui sort de la region de son conteneur. Il reste rouge sur deux familles\n");
		out.Append("       non corrigees : un conteneur a sizeRel 1.0 pose apres une marge, et une\n");
		out.Append("       HBox qui prend toute la largeur du parent depuis un curseur indente\n");
	}

	struct Bilan {
			uint32 n1 = 0, n2 = 0, n3 = 0, total = 0;
	};

} // namespace

int main(int argc, char **argv) {
	const char *fDemandes = "Applications/NKUIDesign/exemples/ia/demandes.txt";
	const char *dSortie = "Build/ia-jeu";
	const char *dorsal = "processus";
	bool sondeHttp = false; // --sonde-http : le TRANSPORT avant le modele
	bool catalogueBref = false; // --catalogue=bref : sans les param/variante
	const char *rejouer = nullptr; // --rejouer=<f> : un texte, sans modele
	const char *migrer = nullptr;  // --migrer=<f> : `enfants` -> `parent`, en place
	// --spec=<texte> : pose une SPECIFICATION dans l'invite, comme le panneau le
	// fait apres « Ecrire la specification ». C'est la variable a eprouver.
	const char *specTexte = nullptr;
	// --document=<f> : le DOCUMENT COURANT sur lequel la demande arrive. Vide par
	// defaut -- et c'est precisement la condition qu'on avait toujours mesuree.
	const char *documentBase = nullptr;
	// --selection=<n> : le noeud que l'utilisateur aurait sous la main. Au-dela du
	// seuil, c'est le SEUL donne en entier dans l'invite ; -1 = aucun.
	int32 selectionBanc = -1;
	// --structure=enfants : l invite d AVANT le 20/09, pour comparer la FORME
	// et rien d autre. Defaut : `parent`, la forme livree.
	bool structureEnfants = false;
	const char *seul = nullptr;		// --seule=d01 : une seule demande
	const char *contrat = nullptr;	// --contrat=<f> : ECRIRE le contrat d'outil
	const char *verifier = nullptr; // --verifier-contrat=<f> : la GARDE anti-derive
	/// Le plafond d'attente du dorsal, en ms. 0 = « sans objet » (dorsal sans
	/// reseau). Il voyage jusqu'a la paire de recolte : une duree egale au
	/// plafond doit pouvoir se lire comme telle des annees plus tard.
	unsigned delaiDorsal = 0u;
	for (int a = 1; a < argc; ++a) {
		if (CommencePar(argv[a], "--demandes="))
			fDemandes = argv[a] + 11;
		else if (CommencePar(argv[a], "--sortie="))
			dSortie = argv[a] + 9;
		else if (std::strcmp(argv[a], "--sonde-http") == 0)
			sondeHttp = true;
		else if (CommencePar(argv[a], "--rejouer="))
			rejouer = argv[a] + 10;
		else if (CommencePar(argv[a], "--migrer="))
			migrer = argv[a] + 9;
		else if (CommencePar(argv[a], "--spec="))
			specTexte = argv[a] + 7;
		else if (CommencePar(argv[a], "--document="))
			documentBase = argv[a] + 11;
		else if (CommencePar(argv[a], "--selection="))
			selectionBanc = (int32)std::atoi(argv[a] + 12);
		else if (std::strcmp(argv[a], "--structure=enfants") == 0)
			structureEnfants = true;
		else if (std::strcmp(argv[a], "--catalogue=bref") == 0)
			catalogueBref = true;
		else if (CommencePar(argv[a], "--dorsal="))
			dorsal = argv[a] + 9;
		else if (CommencePar(argv[a], "--seule="))
			seul = argv[a] + 8;
		// ⚠️ `--verifier-contrat=` SE TESTE AVANT `--contrat=`, parce que le
		//    second est un PREFIXE du premier sur ses neuf premiers caracteres
		//    et l'attraperait. Une analyse d'arguments ou un drapeau en avale un
		//    autre est un piege muet : l'option demandee ne s'execute jamais, et
		//    rien ne le dit.
		else if (CommencePar(argv[a], "--verifier-contrat="))
			verifier = argv[a] + 19;
		else if (CommencePar(argv[a], "--contrat="))
			contrat = argv[a] + 10;
		else if (std::strcmp(argv[a], "--aide") == 0) {
			std::printf("NKDesignIABanc --demandes=<f> --sortie=<dossier> "
						"[--dorsal=processus|temoin|ollama] [--seule=<id>]\n"
						"  processus : le gabarit de NK_DESIGN_CMD / NK_DESIGN_EXE "
						"(NKDesignLLM)\n"
						"  temoin    : LE ZERO du banc -- une reponse fixe et juste\n"
						"  ollama    : un VRAI modele, par le service HTTP local (le modele et "
						"              l'hote sont des REGLAGES : NK_OLLAMA_MODELE, NK_OLLAMA_HOTE)\n"
						"  --contrat=<f>           ecrit le CONTRAT D'OUTIL (sans dorsal,\n"
						"                          sans modele, sans carte graphique)\n"
						"  --verifier-contrat=<f>  la GARDE : rend 1 si le fichier versionne\n"
						"                          ne dit plus ce que le binaire fait\n");
			return 0;
		}
	}

	PeuplerCatalogue();

	// ⚠️ POSE ICI, ET PAS PLUS BAS. Le contrat est ENGENDRE depuis `BuildPrompt` :
	//    si ce reglage arrivait apres le bloc du contrat, `--structure=enfants
	//    --contrat=<f>` ecrirait le contrat de l'AUTRE forme sans rien dire.
	//    *Un reglage pose apres son premier lecteur est un reglage sans effet,
	//    et il n'en previent personne.*
	NkDesignAI::structureParent = !structureEnfants;


	// LE CONTRAT SE REND AVANT TOUTE COURSE, ET C'EST TOUT L'INTERET : il ne
	// demande ni dorsal, ni modele, ni carte graphique. C'est le point du cap --
	// un outil se DECRIT et se MESURE sans le modele, sinon un bon outil et un
	// mauvais modele rendent le meme vert.
	if (contrat || verifier) {
		NkString c;
		BatirContrat(c);
		if (contrat) {
			NkFile::WriteAllText(NkPath(contrat), c);
			std::printf("contrat ecrit : %s (%u octets)\n", contrat, (uint32)c.Size());
		}
		if (verifier) {
			const NkString sur = NkFile::ReadAllText(NkPath(verifier));
			// ⚠️ ON NORMALISE LES FINS DE LIGNE DES DEUX COTES. Le depot est
			//    heterogene -- des fichiers en LF et en CRLF dans les memes
			//    dossiers, mesure le 17/09 -- et git reecrit les fins de ligne a
			//    la sortie de l'index. Une garde qui rougit sur un CRLF crie sans
			//    rien dire, et on apprend a l'ignorer : c'est ainsi qu'un vrai
			//    rouge passe inapercu.
			NkString a, b;
			for (uint32 i = 0; i < (uint32)c.Size(); ++i)
				if (c.Data()[i] != '\r')
					a.Append(c.Data()[i]);
			for (uint32 i = 0; i < (uint32)sur.Size(); ++i)
				if (sur.Data()[i] != '\r')
					b.Append(sur.Data()[i]);
			const bool ok = a.Size() == b.Size() && a.Compare(b) == 0;
			std::printf("GARDE DU CONTRAT : %s\n"
						"  engendre %u octets, versionne %u octets\n",
						ok ? "IDENTIQUE"
						   : "A DERIVE -- le fichier versionne ne dit plus ce que le binaire fait",
						(uint32)a.Size(), (uint32)b.Size());
			if (!ok)
				return 1;
		}
		return 0;
	}

	// ── SONDE DU TRANSPORT : `NkHTTPClient` PARLE-T-IL REELLEMENT ? ──────────
	// ⚠️ ELLE EXISTE PARCE QUE « DECLARER N'EST PAS LIVRER ». `NkHTTPClient` a
	//    1 582 lignes d'en-tete, un `.cpp`, et des exemples en commentaire. Rien
	//    de tout ca ne prouve qu'un POST aboutit. Avant d'accuser un modele ou
	//    une invite, on demande au TRANSPORT ce qu'il rend, et on l'imprime.
	// ── REJOUER UN TEXTE SANS RAPPELER UN MODELE ─────────────────────────────
	// ⚠️ `DesignAI.h` PROMET DEJA CETTE CAPACITE : « `Apply` est separee de
	//    `Ask` pour une raison pratique : c'est ce qui permet de rejouer un
	//    texte suspect autant de fois qu'on veut, sans rappeler un modele et
	//    sans payer un jeton. » Elle n'avait aucune porte en ligne de commande.
	//
	//    Elle repond a une question qu'aucune course ne tranche : quand un
	//    document est rejete, EST-CE POUR LA RAISON QU'ON CROIT ? Une reponse
	//    peut echouer sur la STRUCTURE avant meme que le nom des composants
	//    soit regarde -- et on attribuerait alors le rejet au mauvais gardien.
	// ── LA MIGRATION `enfants` -> `parent` ───────────────────────────────────
	// ⚠️ ELLE PASSE PAR LE VRAI LECTEUR ET LE VRAI ECRIVAIN. Un script qui
	//    transformerait le texte a cote serait une SECONDE ECRITURE de la regle
	//    de format : elle divergerait au premier champ ajoute, et c'est la faute
	//    que ce depot a deja payee ailleurs.
	//
	// ⚠️ ET ELLE A UN POUVOIR D'ARRET. Elle relit ce qu'elle s'apprete a ecrire
	//    et compare les comptes AVANT de toucher au fichier : un controle qui
	//    vit dans la meme commande que l'action n'est une garde que s'il peut
	//    empecher l'action. Sinon c'est de la decoration.
	if (migrer) {
		const NkString txt = NkFile::ReadAllText(NkPath(migrer));
		if (txt.Size() == 0) {
			std::printf("MIGRATION : %s est vide ou illisible. RIEN ecrit.\n", migrer);
			return 2;
		}
		PeuplerCatalogue();
		NkUIDocument avant;
		if (!avant.Load(txt.CStr())) {
			std::printf("MIGRATION : %s ne se charge pas AVANT migration. RIEN ecrit.\n", migrer);
			return 2;
		}
		const uint32 nAvant = avant.NodeCount();
		NkString apresTexte;
		avant.Save(apresTexte);
		NkUIDocument apres;
		if (!apres.Load(apresTexte.CStr())) {
			std::printf("MIGRATION : %s -- la relecture de ce qu'on allait ecrire ECHOUE. "
						"RIEN ecrit.\n", migrer);
			return 1;
		}
		const uint32 nApres = apres.NodeCount();
		// Les comptes, ET la forme de l'arbre : un meme nombre de noeuds ranges
		// autrement passerait un simple comptage. *Un compteur egal n'est pas un
		// arbre identique.*
		bool memeArbre = (nAvant == nApres);
		for (uint32 i = 0; memeArbre && i < nAvant; ++i)
			memeArbre = (avant.nodes[i].parent == apres.nodes[i].parent) &&
						(avant.nodes[i].children.Size() == apres.nodes[i].children.Size());
		if (!memeArbre) {
			std::printf("MIGRATION : %s -- l'arbre CHANGE (%u noeuds avant, %u apres, ou des "
						"liens differents). RIEN ecrit.\n", migrer, nAvant, nApres);
			return 1;
		}
		if (!NkFile::WriteAllText(NkPath(migrer), apresTexte.CStr())) {
			std::printf("MIGRATION : %s -- ecriture impossible. RIEN ecrit.\n", migrer);
			return 2;
		}
		std::printf("MIGRATION OK : %-58s %u noeuds, arbre identique\n", migrer, nAvant);
		return 0;
	}

	if (rejouer) {
		const NkString txt = NkFile::ReadAllText(NkPath(rejouer));
		if (txt.Size() == 0) {
			std::printf("REJEU : %s est vide ou illisible. Rien n'a ete mesure.\n", rejouer);
			return 2;
		}
		NkDesignAI iaR;
		PeuplerCatalogue();
		NkUIDocument docR;
		docR.NewDocument("Rejeu", NkAuthor::Humain);
		// ⚠️ LE REJEU PEUT DESORMAIS PARTIR D'UN DOCUMENT. Sans ca il ne pouvait
		//    pas eprouver un INCREMENT : un delta n'a de sens que contre un
		//    document existant, et le rejeu aurait mesure le mauvais chemin.
		if (documentBase && *documentBase) {
			const NkString dt = NkFile::ReadAllText(NkPath(documentBase));
			if (dt.Size() == 0 || !docR.Load(dt.CStr())) {
				std::printf("DOCUMENT DE BASE ILLISIBLE : %s -- rien mesure.\n", documentBase);
				return 2;
			}
		}
		const NkAIResult rr = iaR.Apply(txt.CStr(), docR, 0, "rejeu");
		std::printf("REJEU de %s (%u octets)\n", rejouer, (uint32)txt.Size());
		std::printf("  verdict : %s\n", NkAIVerdictName(rr.verdict));
		std::printf("  detail  : %s\n", rr.detail.Size() > 0 ? rr.detail.CStr() : "(aucun)");
		std::printf("  noeuds ajoutes : %u ; composants inconnus : %u\n",
				 rr.nodesAdded, rr.unknownComponents);
		return rr.Accepted() ? 0 : 1;
	}

	if (sondeHttp) {
		static NkOllamaBackend o;
		if (const char *h = std::getenv("NK_OLLAMA_HOTE"))
			o.hote = NkString(h);
		nkentseu::net::NkHTTPClient http;
		nkentseu::net::NkHTTPClient::Config cfg;
		cfg.defaultTimeoutMs = 10000u;
		http.Configure(cfg);
		NkString u = o.hote;
		u.Append("/api/tags");
		const nkentseu::net::NkHTTPResponse g = http.Get(u.Data());
		std::printf("SONDE HTTP  GET  %s\n", u.Data());
		std::printf("   statusCode = %u   octets de corps = %u   duree = %u ms\n",
				   (unsigned)g.statusCode, (unsigned)g.body.Size(), (unsigned)g.timeMs);
		std::printf("   erreur reseau = \"%s\"\n", g.error.Data());
		NkString u2 = o.hote;
		u2.Append("/api/generate");
		const nkentseu::net::NkHTTPResponse pr =
			http.Post(u2.Data(), "{\"model\":\"qwen2.5:7b-instruct\",\"prompt\":\"OK\",\"stream\":false}");
		std::printf("SONDE HTTP  POST %s\n", u2.Data());
		std::printf("   statusCode = %u   octets de corps = %u   duree = %u ms\n",
				   (unsigned)pr.statusCode, (unsigned)pr.body.Size(), (unsigned)pr.timeMs);
		std::printf("   erreur reseau = \"%s\"\n", pr.error.Data());
		return 0;
	}

	NkDirectory::CreateRecursive(dSortie);

	Ligne demandes[64];
	const uint32 nbD = LireDemandes(fDemandes, demandes, 64);
	if (nbD == 0) {
		std::printf("AUCUNE DEMANDE LUE dans %s -- rien n'a ete mesure.\n", fDemandes);
		return 2;
	}

	// ── LE DORSAL ────────────────────────────────────────────────────────────
	DorsalTemoin temoin;
	// ⚠️ STATIQUE, PARCE QUE `ia` GARDE UN POINTEUR. Ce depot a paye la faute
	//    inverse : « le registre garde un pointeur -- declarer par valeur =
	//    segfault mouvant ». Le dorsal doit survivre a la portee ou il est pose.
	static NkOllamaBackend ollama;
	NkDesignAI ia;
	if (std::strcmp(dorsal, "temoin") == 0) {
		ia.SetBackend(&temoin);
	} else if (std::strcmp(dorsal, "ollama") == 0) {
		// Le modele et l'hote sont des REGLAGES : lus dans l'environnement plutot
		// que graves. NK_OLLAMA_MODELE et NK_OLLAMA_HOTE.
		if (const char *m = std::getenv("NK_OLLAMA_MODELE"))
			ollama.modele = NkString(m);
		if (const char *h = std::getenv("NK_OLLAMA_HOTE"))
			ollama.hote = NkString(h);
		// ⚠️ TEMPERATURE 0 = MESURE REPRODUCTIBLE. Sans elle, deux courses
		//    identiques rendent des taux differents et l'ecart entre deux invites
		//    se noie dans le tirage au sort.
		if (const char *tp = std::getenv("NK_OLLAMA_TEMP"))
			ollama.temperature = (float32)atof(tp);
		// ⚠️ LE PLAFOND D'ATTENTE DEVIENT UN REGLAGE, ET IL EST IMPRIME.
		//    Grave a 300 000 ms, il a tranche DEUX demandes des courses A du 19/09
		//    (`d10`, `duree_ms = 300 041`) sans que rien ne le dise : on a lu « le
		//    modele echoue » la ou il fallait lire « on a cesse d'attendre ».
		//    ⚠️ `atof` et non `atoi` serait un piege fr-FR ; ici c'est un entier,
		//    et la valeur lue est REIMPRIMEE pour qu'un reglage muet ne passe pas.
		if (const char *dl = std::getenv("NK_OLLAMA_DELAI")) {
			const int v = std::atoi(dl);
			if (v > 0)
				ollama.delaiMs = (uint32)v;
		}
		delaiDorsal = (unsigned)ollama.delaiMs;
		// ⚠️ ON INTERROGE LE SERVICE AVANT DE LANCER DOUZE DEMANDES. Sans ca, un
		//    service eteint rendrait douze refus identiques et on lirait « le
		//    modele echoue » la ou il faut lire « personne n'ecoute ».
		if (!ollama.IsAvailable()) {
			std::printf("SERVICE INJOIGNABLE : %s ne repond pas (code %u). Rien mesure.\n",
					ollama.hote.Data(), (unsigned)ollama.dernierCode);
			return 2;
		}
		std::printf("modele        : %s (reglage) sur %s\n", ollama.modele.Data(), ollama.hote.Data());
		std::printf("delai max     : %u ms (reglage NK_OLLAMA_DELAI) -- une duree egale\n"
					"                a ce plafond dit qu'ON A CESSE D'ATTENDRE, pas que le\n"
					"                modele a echoue\n",
					(unsigned)ollama.delaiMs);
		ia.SetBackend(&ollama);
	} else {
		NkDesignBackendProcessus &proc = NkDesignBackendProcessus::ParDefaut();
		char inv[512], sor[512];
		Joindre(inv, sizeof(inv), dSortie, "/invite_courante.txt");
		Joindre(sor, sizeof(sor), dSortie, "/sortie_courante.txt");
		proc.invitePath = NkString(inv);
		proc.sortiePath = NkString(sor);
		if (!proc.IsAvailable()) {
			std::printf("AUCUN GABARIT : pose NK_DESIGN_EXE (chemin de NKDesignLLM.exe) "
						"ou NK_DESIGN_CMD. Rien n'a ete mesure.\n");
			return 2;
		}
		ia.SetBackend(&proc);
	}

	// L'etiquette voyage avec chaque paire : voir Recolte.h.
	NkString etiquetteCatalogue;
	ia.catalogueBref = catalogueBref;
	ia.selectionCourante = selectionBanc;
	// ⚠️ LA SPECIFICATION EST BATIE PAR SON PROPRE ECRIVAIN, pas recopiee ici.
	//    `DepuisConversation` puis `PourLeGenerateur` sont exactement ce que le
	//    panneau appelle apres « Ecrire la specification » : les exigences sont
	//    les TOURS DE L'HUMAIN. La reecrire a la main ici en ferait une seconde
	//    version, et on mesurerait ma reconstitution au lieu de l'outil.
	if (specTexte && *specTexte) {
		nkuidesign::NkDesignConversation conv;
		conv.sujet = NkString(specTexte);
		conv.Ajouter(nkuidesign::NkQui::Moi, specTexte);
		nkuidesign::NkSpecification sp;
		nkuidesign::NkSpecification::DepuisConversation(conv, "spec", sp);
		sp.PourLeGenerateur(ia.specTexte);
		std::printf("specification  : %u exigence(s), %u octets dans l'invite\n",
					sp.CountExigences(), (unsigned)ia.specTexte.Size());
	}
	NkString catalogue;
	NkDesignAI::BuildCatalog(catalogue, catalogueBref);
	uint32 nbComposants = 0;
	for (uint32 i = 0; i < (uint32)catalogue.Size(); ++i)
		if (catalogue.Data()[i] == '\n' && i + 10 < (uint32)catalogue.Size()
			&& CommencePar(catalogue.Data() + i + 1, "composant "))
			++nbComposants;
	if (catalogue.Size() > 0 && CommencePar(catalogue.Data(), "composant "))
		++nbComposants;

	std::printf("=== JEU D'EPREUVE DE L'IA DE DESIGN ===\n");
	std::printf("dorsal        : %s\n", ia.Backend()->Name());
	// ⚠️ LA CONDITION DE LA COURSE, CAPTUREE UNE FOIS. Une paire de recolte qui
	//    ne porte pas son modele est un chiffre sans sa condition -- faute payee
	//    trois fois par ce depot. On la fixe ici, pas dans la boucle.
	// Un dorsal sans modele le dit en un mot : le nom du fichier doit rester
	// triable, et `[condition]` porte deja le dorsal.
	NkString modeleCourant = NkString("aucun");
	if (std::strcmp(dorsal, "ollama") == 0)
		modeleCourant = ollama.modele;
	std::printf("demandes      : %u (lues dans %s)\n", nbD, fDemandes);
	// ⚠️ LE NIVEAU DU CATALOGUE EST UNE CONDITION DE LA MESURE : il s'imprime
	//    et il entre dans la recolte. Un taux qui ne dit pas avec quel
	//    catalogue il a ete obtenu est un chiffre sans sa condition.
	std::printf("catalogue     : %s, %u octets\n",
		   catalogueBref ? "BREF (sans param/variante)" : "COMPLET",
		   (uint32)catalogue.Size());
	{
		char eb[64];
		snprintf(eb, sizeof(eb), "%s, %u octets", catalogueBref ? "bref" : "complet",
				 (unsigned)catalogue.Size());
		etiquetteCatalogue = NkString(eb);
	}
	std::printf("catalogue     : %u composant(s) declares au registre\n", nbComposants);
	std::printf("sortie        : %s\n\n", dSortie);

	Bilan b;
	for (uint32 i = 0; i < nbD; ++i) {
		const Ligne &d = demandes[i];
		if (seul && std::strcmp(seul, d.id) != 0)
			continue;
		++b.total;

		// ⚠️ LE CHEMIN SEULEMENT : le FICHIER s'ecrit APRES l'appel, depuis les
		//    octets que le dorsal a reellement envoyes.
		//
		//    Ce bloc REBATISSAIT l'invite de son cote. Deux ecritures de la meme
		//    regle, donc deux verites -- et elles avaient DIVERGE : mesure du
		//    20/09 sur `e10`, 3 332 caracteres envoyes contre 2 693 ecrits. Il
		//    manquait un saut de ligne et **tout le bloc `document courant`**.
		//    *Le fichier ne prouvait rien, et il piegeait quiconque le rejouait --
		//    moi le premier, la meme nuit.*
		char cheminInv[512];
		char nomInv[64];
		Joindre(nomInv, sizeof(nomInv), "/", d.id);
		char nomInv2[80];
		Joindre(nomInv2, sizeof(nomInv2), nomInv, "_invite.txt");
		Joindre(cheminInv, sizeof(cheminInv), dSortie, nomInv2);

		// ── N1 : le dorsal rend-il quelque chose ? ───────────────────────────
		// ⚠️ LE DOCUMENT COURANT ENTRE DANS L'INVITE (`doc.Save(req.currentDoc)`),
		//    et le banc a TOUJOURS mesure sur un document VIDE. On mesurait donc
		//    la generation a vide et on la livrait sur un document plein : le
		//    20/09, celui de Rodolf portait 42 noeuds, et le modele s'est mis a
		//    DECRIRE ces noeuds au lieu de repondre a la demande.
		//    `--document=<f>` met le banc dans la condition reelle.
		NkUIDocument doc;
		doc.NewDocument("Jeu", NkAuthor::Humain);
		if (documentBase && *documentBase) {
			const NkString t = NkFile::ReadAllText(NkPath(documentBase));
			// ⚠️ ON REFUSE PLUTOT QUE DE MESURER A VIDE SANS LE DIRE. Un document
			//    qui ne se charge pas laisserait la course tourner sur un document
			//    NEUF, et le chiffre porterait l'autre condition sous le meme nom.
			if (t.Size() == 0 || !doc.Load(t.CStr())) {
				std::printf("DOCUMENT DE BASE ILLISIBLE : %s -- rien mesure.\n", documentBase);
				return 2;
			}
		}
		NkChrono chrono;
		chrono.Reset();
		const NkAIResult res = ia.Ask(d.texte, doc, 0);
		const float64 msEcoule = chrono.Elapsed().ToMilliseconds();

		// ── L'INVITE : LES OCTETS ENVOYES, OU RIEN ──────────────────────────
		// ⚠️ `WriteAllBytes` ET NON `WriteAllText` : l'ecriture texte convertit
		//    les sauts de ligne en CRLF, et le fichier cesserait d'etre identique
		//    AU BIT a ce qui est parti sur le reseau. Le temoin de ce correctif
		//    est precisement cette identite-la.
		// ⚠️ ET SI LE DORSAL N'A RIEN BATI (le temoin), ON N'ECRIT RIEN. Un
		//    fichier reconstitue serait la faute qu'on vient de retirer :
		//    *un fichier absent est honnete, un fichier faux ne l'est pas.*
		if (ia.Backend()) {
			const NkString &envoye = ia.Backend()->DerniereInvite();
			if (envoye.Length() > 0) {
				NkVector<uint8> octets;
				for (NkString::SizeType k = 0; k < envoye.Length(); ++k)
					octets.PushBack((uint8)envoye.Data()[k]);
				NkFile::WriteAllBytes(NkPath(cheminInv), octets);
			}
		}

		const NkString &brut = ia.LastReply();
		char cheminRep[512], nomRep[80];
		Joindre(nomRep, sizeof(nomRep), nomInv, "_reponse.txt");
		Joindre(cheminRep, sizeof(cheminRep), dSortie, nomRep);
		if (brut.Size() > 0)
			NkFile::WriteAllText(NkPath(cheminRep), brut);

		const bool n1 = brut.Size() > 0;
		if (n1)
			++b.n1;
		const bool n2 = res.Accepted();
		if (n2)
			++b.n2;

		// ── N3 : le document s'ouvre-t-il ? ──────────────────────────────────
		// Greffe faite (Ask pose deja), mise en page, puis `.nkgui` sur le
		// disque. Le MONTAGE est l'affaire de l'autre binaire.
		// ⚠️ DECLARE A LA PORTEE DE LA PAIRE, pas dans le `if (n2)` : la RECOLTE
		//    en a besoin apres, et une variable enfermee dans la branche du succes
		//    aurait force a recalculer le chemin ailleurs -- deux verites sur le
		//    meme fichier.
		char cheminG[512] = {0};
		bool n3 = false;
		uint32 lignesNkgui = 0;
		uint32 rolesDuComposant = 0;
		uint32 composantsSansRole = 0;
		if (n2) {
			NkPaintRect sfc = {0.f, 0.f, 1200.f, 800.f};
			NkLayoutResult lay;
			NkComputeLayout(doc, sfc, lay);
			char nomG[80];
			Joindre(nomG, sizeof(nomG), nomInv, ".nkgui");
			Joindre(cheminG, sizeof(cheminG), dSortie, nomG);
			guifmt::NkEcritRapport rap;
			n3 = guifmt::NkEcrireNkgui(doc, lay, cheminG, rap);
			rolesDuComposant = rap.rolesDuComposant;
			composantsSansRole = rap.composantsSansRole;
			if (n3) {
				const NkString t = NkFile::ReadAllText(NkPath(cheminG));
				for (uint32 k = 0; k < (uint32)t.Size(); ++k)
					if (t.Data()[k] == '\n')
						++lignesNkgui;
			}
		}
		if (n3)
			++b.n3;

		std::printf("%-4s N1 %s (%u o, %.0f ms) | N2 %-42s | N3 %s", d.id, n1 ? "oui" : "NON",
					(uint32)brut.Size(), msEcoule, NkAIVerdictName(res.verdict),
					n3 ? "oui" : (n2 ? "NON" : "-"));
		// ⚠️ LE REFUS SE PUBLIE A COTE DU SUCCES. Un composant du registre sans
		//    equivalent dans le vocabulaire `.nkgui` (les composites du kit) sort
		//    en CONTENEUR : le document se monte et ne montre pas ce qui a ete
		//    demande. Taire ce compte rendrait N3 vert sur un document creux --
		//    exactement le defaut que la course du temoin a trouve le 18/09.
		if (n3) {
			std::printf(" (%u noeuds, %u lignes de .nkgui, %u role(s) tire(s) du composant",
						res.nodesAdded, lignesNkgui, rolesDuComposant);
			if (composantsSansRole > 0)
				std::printf(", %u composant(s) SANS ROLE", composantsSansRole);
			std::printf(")");
		}
		if (!n2 && res.detail.Size() > 0)
			std::printf("  [%s]", res.detail.CStr());

		// ── LA RECOLTE ───────────────────────────────────────────────────────
		// ⚠️ ICI, ET PAS AILLEURS : c'est le seul endroit ou les trois verdicts
		//    existent en meme temps que la reponse brute et le document. Les
		//    recalculer plus loin aurait fait deux verites sur la meme paire.
		//
		// ⚠️ AUCUNE CONDITION SUR n2 NI n3. On ecrit la paire QUOI QU'IL ARRIVE :
		//    un corpus qui ne contient que des reussites n'apprend pas a refuser.
		{
			NkString docProduit;
			if (n3)
				docProduit = NkFile::ReadAllText(NkPath(cheminG));
			nkrecolte::NkPaire paire;
			paire.id = d.id;
			paire.demande = d.texte;
			paire.dorsal = ia.Backend()->Name();
			paire.modele = modeleCourant.CStr();
			paire.verdictNom = NkAIVerdictName(res.verdict);
			paire.motif = res.detail.Size() > 0 ? res.detail.CStr() : "";
			paire.n1 = n1;
			paire.n2 = n2;
			paire.n3 = n3;
			paire.ms = msEcoule;
			paire.delaiMs = delaiDorsal;
			paire.catalogue = etiquetteCatalogue.CStr();
			// La provenance du JEU DE DEMANDES, pas de la reponse : le fichier des
			// demandes est a nous, ecrit avant la premiere course et jamais modifie.
			paire.provenance = fDemandes;
			paire.brut = &brut;
			paire.document = &docProduit;
			char dRecolte[512];
			Joindre(dRecolte, sizeof(dRecolte), dSortie, "/recolte");
			// Une recolte qui echoue en SILENCE se decouvre le jour ou on veut s'en
			// servir : on le dit tout de suite, sur la meme ligne que le verdict.
			if (!nkrecolte::Ecrire(dRecolte, paire))
				std::printf("  [RECOLTE NON ECRITE]");
		}
		std::printf("\n");
		std::fflush(stdout);
	}

	std::printf("\n=== TAUX, sur %u demande(s) ===\n", b.total);
	std::printf("  N1  le modele rend quelque chose : %u / %u\n", b.n1, b.total);
	std::printf("  N2  le document se LIT           : %u / %u\n", b.n2, b.total);
	std::printf("  N3  le document S'OUVRE (.nkgui) : %u / %u\n", b.n3, b.total);
	std::printf("\nLe MONTAGE se mesure a part : NKGuiMonteTest --monter=<fichier.nkgui>\n");
	return 0;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
