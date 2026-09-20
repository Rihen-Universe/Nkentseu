#pragma once
// -----------------------------------------------------------------------------
// @File    DesignAI.h
// @Brief   LA PLACE DE L'IA — elle passe par la MEME PORTE que la main.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EST LIVRE ICI, ET CE QUI NE L'EST PAS — a lire en premier
// =============================================================================
//  ⚠️ **LE MODELE SPECIALISE N'EXISTE PAS.** Il s'entrainera sur des
//     declarations, et il n'y en a presque pas encore. C'est la boucle posee par
//     Rodolf : l'outil produit le corpus qui entraine le modele. Ce fichier ne
//     contient donc AUCUNE intelligence, et n'en revendique aucune.
//
//  CE QUI EST LIVRE, c'est **la place** — et elle coute quelques dizaines de
//  lignes aujourd'hui contre une refonte plus tard :
//    1. un point d'entree de prompt, en francais ;
//    2. un backend REMPLACABLE (Claude, Ollama, Ilyana demain) derriere une
//       interface qui ne suppose rien de l'un ni de l'autre ;
//    3. ⚠️ **la sortie de l'IA passe par la MEME PORTE que la main** : elle
//       produit un DOCUMENT, qui atterrit dans l'arbre par `GraftFrom`, la
//       fonction qu'utilise aussi le copier-coller. Jamais du code, jamais un
//       artefact a part ;
//    4. la provenance se remplit toute seule (`auteur = ia`), et la correction
//       humaine ensuite aussi (`MarkHumanEdit`) — c'est la que le corpus se
//       fabrique, sans effort supplementaire ;
//    5. **le rejeu comme verificateur** : une proposition qui ne se rejoue pas a
//       l'identique est ECARTEE. Le moteur est le juge, pas l'oeil ;
//    6. depuis le 30/08, **l'apercu et le retrait** (spec §6.2, garantie 1) :
//       `Propose` garde la proposition validee DE COTE sans toucher au document,
//       `CommitProposal` la pose par la meme porte, `DiscardProposal` la jette,
//       et `Retract` retire ENTIEREMENT une greffe posee — l'annulation d'un
//       geste, en attendant un historique de document qui n'existe pas encore.
//
//  CE QUI N'EST PAS LIVRE, ET QUI EST NOMME :
//    - **aucun backend reseau.** PV3DE parle a Claude et a Ollama par des
//      sockets ecrites a la main (`Applications/PV3DE/src/PV3DE/AI/Backends/`,
//      « en production : remplacer par NKStream::HttpClient quand disponible »).
//      Recopier ces sockets ici en ferait une troisieme copie, dans une
//      application, alors que la directive du depot est exactement l'inverse.
//      **Le client HTTP doit monter dans un module partage** — porte au canal.
//      En attendant, le backend FICHIER ci-dessous n'est pas un pis-aller : il
//      rend l'outil utilisable des aujourd'hui avec n'importe quel modele, y
//      compris a la main.
//    - aucune conversation multi-tours, aucun streaming, aucun asynchrone. La
//      requete est synchrone et le restera tant qu'aucun backend ne bloque
//      vraiment.
//
// =============================================================================
//  POURQUOI LE REJEU EST LA PIECE QUI COMPTE
// =============================================================================
//  Regle du corpus : *« une paire fausse est apprise fidelement »*. Un corpus
//  synthetique non verifie est pire qu'un petit corpus vrai. Ici le verificateur
//  existe deja et il est mecanique : une declaration se **rejoue**.
//
//      texte propose -> document -> reserialise -> relu -> MEME mise en page ?
//
//  Ce que ce controle attrape reellement : une proposition qui ne survit pas a
//  un aller-retour par le format — reference inventee, structure incoherente,
//  valeur que l'ecrivain ne sait pas reecrire. Ce sont exactement les fautes
//  qu'un modele produit quand il ecrit « a peu pres » le bon format.
//
//  ⚠️ CE QU'IL N'ATTRAPE PAS, ET IL FAUT LE DIRE AVEC LUI : une interface
//     PARFAITEMENT bien formee et LAIDE passe le rejeu sans broncher. Le rejeu
//     verifie la FIDELITE, pas la qualite. Le jugement esthetique reste humain,
//     et c'est precisement pourquoi `corrected` existe.
//
// OU AJOUTER LA PROCHAINE CHOSE :
//   un backend de plus -> une classe qui herite de `NkIDesignBackend`, et rien
//   d'autre a toucher : ni le prompt, ni la porte, ni la provenance, ni la
//   sonde.
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif

#include "NKConverse/NkConverse.h"
#include "NKConverse/NkConverseOllama.h" // le transport : invite, reponse, dorsaux
#include "NKConverse/NkConverseClaude.h" // le CINQUIEME dorsal, et le seul DISTANT
#include "Layout.h"

namespace nkuidesign {

	// ── CE QUI VIVAIT ICI HABITE DESORMAIS NKConverse ───────────────────────
	// La requete, la reponse, l'interface de dorsal et les TROIS dorsaux
	// (fichier, processus, conserve) sont descendus dans
	// `Kernel/System/NKConverse` : ils ne savaient rien du document, et le meme
	// patron etait deja ecrit trois fois dans la maison (PV3DE, ici, NKCode).
	//
	// ⚠️ DEMENAGER, PAS SUPPRIMER. Les noms d'ici restent des noms d'ici : ces
	//    alias existent pour que PAS UNE LIGNE des appelants ne bouge. Le jour
	//    ou quelqu'un travaille dans ce fichier, il peut ecrire les noms du
	//    module directement et retirer l'alias correspondant — un par un, jamais
	//    tous d'un coup.
	//
	// ⚠️ ET CE QUI N'EST PAS DESCENDU, VOLONTAIREMENT : `NkAIVerdict` et
	//    `NkAIResult`, juste en dessous. Ils COMPILENT sans document, mais ils
	//    en PARLENT — « aucun document lisible », « composant que le registre
	//    ignore », « cible de greffe invalide », `graftedRoot`, `nodesAdded`.
	//    Les descendre aurait fait entrer le vocabulaire du design dans un
	//    module qui doit l'ignorer. La coupe est donc a 383, pas a 429.
	using NkDesignRequest = nkentseu::converse::NkConverseRequest;
	using NkDesignReply = nkentseu::converse::NkConverseReply;
	using NkIDesignBackend = nkentseu::converse::NkIConverseBackend;
	using NkFileBackend = nkentseu::converse::NkConverseBackendFichier;
	using NkDesignBackendProcessus = nkentseu::converse::NkConverseBackendProcessus;
	using NkCannedBackend = nkentseu::converse::NkConverseBackendConserve;
	/// ⚠️ LE QUATRIEME DORSAL, et le premier qui parle a un VRAI modele sur
	///    cette machine. Il vit dans son propre en-tete parce qu'il est le seul
	///    a tirer NKNetwork -- un consommateur qui ne veut que le dorsal fichier
	///    n'a pas a payer la pile reseau.
	using NkOllamaBackend = nkentseu::converse::NkConverseBackendOllama;
	/// ⚠️ LE CINQUIEME DORSAL, ET LE SEUL QUI SORTE DE LA MACHINE. Les quatre
	///    autres travaillent en local -- fichier, processus, conserve, Ollama.
	///    Celui-ci envoie l'invite chez Anthropic, ET L'INVITE DU DESIGN PORTE
	///    LE DOCUMENT COURANT (`NkConverseRequest::currentDoc`). Ce n'est donc
	///    pas « une phrase qui part » : c'est la maquette entiere. L'interface
	///    doit le dire AVANT l'usage, et c'est la seule raison pour laquelle ce
	///    dorsal n'est jamais choisi automatiquement (voir `Init`).
	using NkClaudeBackend = nkentseu::converse::NkConverseBackendClaude;

	enum class NkAIVerdict : uint8 {
		Acceptee = 0,
		BackendMuet,	 ///< le backend n'a rien rendu
		TexteNonConforme, ///< pas de document lisible dans la reponse
		ComposantInconnu, ///< nomme un composant que le registre ignore
		RejeuDivergent,	  ///< se charge, mais ne survit pas a l'aller-retour
		GreffeRefusee,	  ///< la cible n'accepte pas (index invalide, cycle)
		Count
	};

	inline const char *NkAIVerdictName(NkAIVerdict v) {
		switch (v) {
			case NkAIVerdict::Acceptee:
				return "acceptee";
			case NkAIVerdict::BackendMuet:
				return "le backend n'a rien rendu";
			case NkAIVerdict::TexteNonConforme:
				return "aucun document lisible dans la reponse";
			case NkAIVerdict::ComposantInconnu:
				return "nomme un composant que le registre ignore";
			case NkAIVerdict::RejeuDivergent:
				return "ne se rejoue pas a l'identique";
			case NkAIVerdict::GreffeRefusee:
				return "cible de greffe invalide";
			default:
				return "?";
		}
	}

	struct NkAIResult {
			NkAIVerdict verdict = NkAIVerdict::BackendMuet;
			int32 graftedRoot = -1; ///< index du sous-arbre pose, -1 si rejet
			uint32 nodesAdded = 0;
			uint32 unknownComponents = 0;
			uint32 replayDiffs = 0; ///< rectangles qui divergent apres aller-retour
			/// ⚠️ LE POINT D'ACCROCHAGE D'UN INCREMENT, -1 si la reponse etait un
			///    document autonome. Il vient de la REPONSE (`parent = 41`), pas de
			///    la selection : le modele dit ou il veut poser, et c'est lui qui a
			///    raison -- la selection peut etre ailleurs, ou nulle part.
			int32 cibleIncrement = -1;
			NkString detail;

			bool Accepted() const {
				return verdict == NkAIVerdict::Acceptee;
			}
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  L'ORCHESTRATEUR
	// ═══════════════════════════════════════════════════════════════════════════
	class NkDesignAI {
		public:
			/// La surface de reference sur laquelle le rejeu se compare. Sa valeur
			/// exacte n'a pas d'importance ; ce qui compte est qu'elle soit LA MEME
			/// des deux cotes de l'aller-retour.
			NkPaintRect replaySurface = {0.f, 0.f, 1200.f, 800.f};

			// -- LA SPECIFICATION, ET C'EST TOUT CE QUE CE FICHIER EN SAIT ----
			// Rodolf : « definir un document de specification LIE A CE DESIGN est
			// important ». La liaison ne demande aucune structure nouvelle : le
			// format porte deja `origine` sur CHAQUE noeud, et `GraftFrom` la
			// remplit avec ce qu'on lui donne. Il suffit donc de lui donner le NOM
			// de la specification au lieu du nom du dorsal.
			//
			// ATTENTION : DEUX CHAMPS, ET LEUR SEPARATION EST LE POINT.
			//   `specTexte`   entre dans l'INVITE  -> il change ce qui est demande
			//   `specOrigine` entre dans la PROVENANCE -> il change ce qu'on
			//                 pourra RETROUVER plus tard
			// Les confondre aurait fait porter aux noeuds un paragraphe entier au
			// lieu d'un nom, et aurait rendu `origine` illisible dans le fichier.
			//
			// Vides tous les deux = le comportement d'avant, au bit pres : la
			// provenance retombe sur `Backend()->Name()`. Aucun appelant existant
			// ne bouge -- les essais 29/30 de la sonde en dependent.
			NkString specTexte;	  ///< les exigences, telles qu'elles partent au dorsal
			NkString specOrigine; ///< le NOM de la specification, pour `origine`

			void SetBackend(NkIDesignBackend *b) {
				mBackend = b;
			}
			/// La provenance a inscrire : le nom de la specification si on en a
			/// une, sinon le nom du dorsal. UNE seule fonction -- deux copies de
			/// cette regle auraient diverge, et une greffe aurait porte une
			/// origine pendant que l'autre en portait une autre.
			const char *OrigineCourante() const {
				if (specOrigine.Length() > 0)
					return specOrigine.Data();
				return mBackend ? mBackend->Name() : "";
			}
			NkIDesignBackend *Backend() const {
				return mBackend;
			}

			// ── LE CATALOGUE, ENGENDRE DEPUIS LE REGISTRE ────────────────────
			// ⚠️ IL BOUCLE SUR LE REGISTRE, il n'enumere aucun nom. C'est la meme
			//    exigence que pour la palette, et pour la meme raison : un
			//    catalogue ecrit a la main proposerait a l'IA des composants
			//    inexistants (ou lui cacherait les nouveaux), et l'outil
			//    « marcherait » en produisant des documents impossibles a poser.
			/// ⚠️ DEUX NIVEAUX DE DETAIL, ET LE CHOIX EST UNE MESURE, PAS UN GOUT.
			///    Le catalogue COMPLET enumere, pour chaque composant, ses variantes,
			///    ses parametres et ses metriques. Mesure du 19/09 : il pese 2 370
			///    octets sur les 4 142 de l'invite (57 %), et **deux composites a eux
			///    seuls en font 1 659 -- soit 40 % de l'invite ENTIERE**.
			///
			///    Ce poids n'est pas gratuit : un modele de vision voit son contexte
			///    mange par l'IMAGE (mesure : ~1 054 jetons sur les 2 048 de
			///    `moondream`). Chaque octet d'invite economise est un octet rendu a
			///    la maquette.
			///
			/// ⚠️ LE BREF RESUME, IL N'AMPUTE PAS. Chaque composant garde son NOM et
			///    sa description ; seules tombent les lignes `variante`, `param` et
			///    `metrique`. *Un composant ABSENT ne peut pas etre choisi ; un
			///    composant RESUME, si.* Retirer des noms ferait inventer les
			///    manquants, et le rejet « composant inconnu » serait de NOTRE fait.
			/// Le niveau de detail du catalogue envoye au modele. REGLAGE, jamais
			/// une constante : c'est une mesure qui doit trancher, et une mesure a
			/// besoin des deux branches dans le MEME binaire.
			bool catalogueBref = false;

			static void BuildCatalog(NkString &out, bool bref = false) {
				out = NkString("");
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 i = 0; i < n; ++i) {
					const NkComponentDecl *d = NkComponentRegistry::At(i);
					if (!d)
						continue;
					out.Append("composant ");
					out.Append(d->name);
					out.Append(" : ");
					out.Append(d->summary ? d->summary : "");
					out.Append('\n');
					if (bref)
						continue; // le nom et la description suffisent a CHOISIR
					for (uint16 v = 0; v < d->variantCount; ++v) {
						out.Append("    variante ");
						out.Append(d->variants[v].name);
						out.Append('\n');
					}
					for (uint16 p = 0; p < d->paramCount; ++p) {
						out.Append("    param ");
						out.Append(d->params[p].name);
						out.Append('\n');
					}
					for (uint16 m = 0; m < d->metricCount; ++m) {
						out.Append("    metrique ");
						out.Append(d->metrics[m].name);
						out.Append('\n');
					}
				}
			}

			/// Le prompt. Il dit trois choses, et rien de plus : ce qu'on veut, ce
			/// qui existe, et **le format exact attendu** — engendre depuis les
			/// memes fonctions de nom que l'ecrivain, pour qu'il ne puisse pas
			/// deriver du format reel.
			/// ⚠️ LA FORME DE STRUCTURE ANNONCEE PAR L'INVITE, ET POURQUOI ELLE EST
			///    UN REGLAGE PLUTOT QU'UN CHOIX GRAVE.
			///
			///    Les deux invites ne peuvent pas coexister dans un binaire si
			///    l'une remplace l'autre dans le code : la course « avant » et la
			///    course « apres » tourneraient alors sur DEUX binaires, donc sur
			///    deux variables -- la forme ET tout ce qui a change entre les deux
			///    constructions. *Une comparaison a deux variables ne vaut rien*, et
			///    ce depot l'a deja paye.
			///
			///    `false` rend EXACTEMENT l'invite d'avant le 20/09, pour que
			///    l'ecart mesure soit celui de la forme et de rien d'autre.
			///    Le DEFAUT est `true` : c'est la forme livree, et c'est elle que
			///    le contrat engendre.
			static inline bool structureParent = true;

			/// ⚠️ `userAsk` N'ENTRE PLUS ICI — il est RAPPELE EN DERNIER, apres le
			///    document courant, par `EcrireRequete` (champ `demande`).
			///    Mesure du 20/09 : sur un document de 42 noeuds, le bloc
			///    `--- document courant ---` occupait 84 % de l'invite et le
			///    modele repondait au DOCUMENT. La demande est DEPLACEE, pas
			///    dupliquee.
			///    Le parametre reste dans la signature : `--contrat=` s'en sert
			///    pour montrer la ligne au lecteur du contrat.
			static void BuildPrompt(const char *userAsk, NkString &out) {
				(void)userAsk;
				out = NkString("Tu produis une INTERFACE pour NkUIDesign.\n\n");
				out.Append("\nReponds UNIQUEMENT par un document au format ci-dessous,\n");
				out.Append("sans explication, sans balise de code.\n\n");
				out.Append("REGLE ABSOLUE : n'ecris JAMAIS de position ni de coordonnee.\n");
				out.Append("La position se calcule ; tu declares des tailles et un agencement.\n\n");
				out.Append("Format :\n");
				out.Append("nkuidoc 1\n");
				out.Append("titre = <texte>\n");
				out.Append("noeud <numero, en partant de 0 pour la racine>\n");
				out.Append("  libelle = <texte>\n");
				// ⚠️ CETTE LIGNE FABRIQUAIT UN DE NOS PROPRES REJETS. Elle disait
				//    « ou vide pour un cadre » : le modele lisait un NOM apres « pour un »
				//    et ecrivait `composant = cadre`. Mesure du 19/09 : les DEUX paires
				//    qui le font sont rejetees, dont une pour « nomme un composant que le
				//    registre ignore ».
				//
				//    *Le modele n'inventait pas ; c'est l'invite qui nommait.* Meme famille
				//    que la regle `enfants` manquante, et trouvee par la meme methode :
				//    LIRE les reponses brutes au lieu de lire le compteur.
				//
				//    La nouvelle formulation ne met AUCUN nom la ou une valeur est
				//    attendue, et renvoie a l'exemple, qui montre deja le champ vide.
				out.Append("  composant = <un nom de la liste ci-dessous ; LAISSE VIDE si ce\n");
				out.Append("              noeud ne porte aucun composant, comme le noeud 0\n");
				out.Append("              de l'exemple>\n");
				// ⚠️ `parent` A REMPLACE `enfants` LE 20/09, ET LE GAIN N'EST PAS
				//    ACQUIS D'AVANCE. Un noeud qu'on oublie de rattacher n'ecrit
				//    plus un orphelin : il ecrit une SECONDE RACINE, et deux
				//    racines sont refusees -- le meme echec, sous un autre nom.
				//    Le pari est de PROXIMITE : `parent = 0` s'ecrit a cote du
				//    noeud, au moment ou on le cree, au lieu d'une liste a tenir
				//    a distance chez le noeud 0. *C'est plausible, ce n'est pas
				//    demontre, et c'est ce que la course mesure.*
				if (structureParent) {
					out.Append("  parent = <le numero du noeud qui CONTIENT celui-ci ; le noeud 0\n");
					out.Append("           est la racine et n'ecrit PAS cette ligne>\n");
				} else {
					out.Append("  enfants = <numeros de noeuds, separes par des espaces>\n");
				}
				out.Append("  largeur = <");
				AppendModes(out);
				out.Append("> <valeur> <min> <max>\n");
				out.Append("  hauteur = <idem>\n");
				out.Append("  agencement = <");
				AppendKinds(out);
				out.Append(">\n");
				out.Append("  ecart = <px>\n  marge = <px>\n  colonnes = <n, pour la grille>\n");
				out.Append("  alignement = <");
				AppendAligns(out);
				out.Append(">\n");
				out.Append("  reglage param <nom> = <valeur>   (facultatif)\n");
				out.Append("  reglage variante <nom>           (facultatif)\n\n");
				// ⚠️ LA REGLE QUI MANQUAIT, ET ELLE VALAIT NEUF ECHECS SUR DOUZE.
				//    Mesure du 17/09 sur qwen2.5:7b-instruct : N2 = 3/12. En lisant les
				//    reponses BRUTES, les echecs ont tous `enfants =` VIDE sur le noeud 0
				//    pendant que les noeuds 1, 2, 3 existent ; le seul succes lu a
				//    `enfants = 1 2`. Le modele ecrivait les noeuds et ne les RATTACHAIT
				//    pas.
				//
				//    L'invite ne l'avait jamais demande : elle decrivait le CHAMP
				//    (« numeros separes par des espaces ») sans dire la CONTRAINTE. *Ce
				//    n'est pas le modele qui inventait mal, c'est nous qui n'avions pas
				//    dit la regle.* C'est le sens du cap : le modele est un reglage, et
				//    l'outillage -- cette invite en fait partie -- est le produit.
				if (structureParent) {
					out.Append("REGLE ABSOLUE : tout noeud autre que 0 porte une ligne `parent` qui\n");
					out.Append("nomme un noeud existant. Un noeud sans `parent` serait une SECONDE\n");
					out.Append("racine, et le document serait refuse pour structure incoherente.\n");
					out.Append("Le noeud 0 est la racine, et il est le SEUL sans ligne `parent`.\n");
				} else {
					out.Append("REGLE ABSOLUE : tout noeud autre que 0 doit apparaitre dans le champ\n");
					out.Append("`enfants` d'exactement UN autre noeud. Un noeud que personne ne cite\n");
					out.Append("n'existe pas : le document est alors refuse pour structure incoherente.\n");
					out.Append("Le noeud 0 est la racine ; c'est lui qui cite les premiers.\n");
				}
				out.Append("\n");
				// ⚠️ UN EXEMPLE PLUTOT QUE TROIS PHRASES DE PLUS. Une contrainte de
				//    structure se montre mieux qu'elle ne se decrit -- et celui-ci est
				//    minuscule a dessein : assez pour montrer le rattachement, trop
				//    petit pour etre recopie tel quel a la place d'une vraie reponse.
				out.Append("Exemple complet et minimal :\n");
				out.Append("nkuidoc 1\n");
				out.Append("titre = Exemple\n");
				out.Append("noeud 0\n");
				out.Append("  libelle = Racine\n");
				out.Append("  composant = \n");
				if (!structureParent)
					out.Append("  enfants = 1 2\n");
				out.Append("  largeur = expand 1 0 0\n");
				out.Append("  hauteur = expand 1 0 0\n");
				out.Append("  agencement = colonne\n");
				out.Append("noeud 1\n");
				out.Append("  libelle = Titre\n");
				out.Append("  composant = etiquette\n");
				out.Append(structureParent ? "  parent = 0\n" : "  enfants =\n");
				out.Append("  largeur = expand 1 0 0\n");
				out.Append("  hauteur = content 24 0 0\n");
				out.Append("noeud 2\n");
				out.Append("  libelle = Valider\n");
				out.Append("  composant = bouton\n");
				out.Append(structureParent ? "  parent = 0\n" : "  enfants =\n");
				out.Append("  largeur = content 0 0 0\n");
				out.Append("  hauteur = content 32 0 0\n");
				out.Append("\n");
				out.Append("N'emploie que des composants du catalogue ci-dessous.\n");
			}

			/// L'invite COMPLETE : le format, puis la specification si on en a
			/// une. Elle est POSEE APRES le format, jamais avant : le format est
			/// la contrainte dure (une reponse qui ne le suit pas est rejetee par
			/// le rejeu), la specification est le contenu.
			void BatirInviteComplete(const char *userAsk, NkString &out) const {
				BuildPrompt(userAsk, out);
				if (specTexte.Length() > 0) {
					out.Append("\n--- specification a respecter ---\n");
					out.Append(specTexte);
					out.Append("\n");
				}
			}

			// ═══════════════════════════════════════════════════════════════════
			//  DEMANDER, PUIS POSER
			// ═══════════════════════════════════════════════════════════════════
			// ⚠️ `doc` N'EST TOUCHE QU'A LA TOUTE DERNIERE ETAPE, et c'est ce qui
			//    rend « le rejet laisse le document intact » vrai par CONSTRUCTION
			//    plutot que par vigilance. Tout se passe dans `scratch`, un
			//    document de cote. Une version qui aurait pose d'abord et verifie
			//    ensuite aurait eu besoin d'une annulation — et une annulation qui
			//    ne sert que dans le cas d'erreur est une annulation qui n'est
			//    jamais testee.
			NkAIResult Ask(const char *userAsk, NkUIDocument &doc, int32 targetParent) {
				NkAIResult res;
				// ⚠️ LA REMISE A ZERO EST LA PREMIERE LIGNE, ET CE N'EST PAS DE
				//    L'HYGIENE. Sans elle, `mLastReply` garde la reponse de la
				//    demande PRECEDENTE quand celle-ci echoue -- et tout ce qui lit
				//    `LastReply()` croit alors tenir une reponse.
				//
				//    Mesure du 20/09, courses `ia-A1` et `ia-A2` : `d10` (timeout
				//    300 s) et `d11` (corps illisible) n'ont RIEN rendu, et le banc
				//    a ecrit dans `d10_reponse.txt` et `d11_reponse.txt` le texte de
				//    `d09` -- 1 526 octets, identiques au bit. Le compteur `n1`,
				//    defini comme « LastReply non vide », a compte DEUX succes
				//    inexistants par course, soit 4 sur 84 paires.
				//
				//    *Un etat qu'on ne remet pas a zero sur l'echec transforme tout
				//    echec suivant en succes apparent, et il penche toujours du meme
				//    cote : celui qui nous flatte.*
				mLastReply = NkString("");
				if (!mBackend) {
					res.detail = NkString("aucun backend branche");
					return res;
				}
				NkDesignRequest req;
				BatirRequete(userAsk, doc, req); // la MEME requete que `Propose`

				NkDesignReply reply;
				if (!mBackend->Complete(req, reply) || reply.text.Length() == 0) {
					res.verdict = NkAIVerdict::BackendMuet;
					res.detail = reply.error;
					return res;
				}
				mLastReply = reply.text;
				return Apply(reply.text.Data(), doc, targetParent, OrigineCourante());
			}

			/// Poser une reponse deja obtenue. Separee de `Ask` pour une raison
			/// pratique : c'est ce qui permet de rejouer un texte suspect autant de
			/// fois qu'on veut, sans rappeler un modele et sans payer un jeton.
			NkAIResult Apply(const char *replyText, NkUIDocument &doc, int32 targetParent,
							 const char *origin) {
				NkAIResult res;
				if (!doc.IsValidIndex(targetParent)) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					return res;
				}
				NkUIDocument scratch;
				// `&doc` : c'est ce qui ouvre le chemin de l'INCREMENT. Sans lui,
				// `ValidateReply` ne peut pas savoir quels noeuds existent.
				if (!ValidateReply(replyText, scratch, res, &doc))
					return res;
				// ⚠️ LA CIBLE VIENT DE LA REPONSE, PAS DE LA SELECTION. Le modele a
				//    dit `parent = 41` : poser ailleurs serait poser ou personne
				//    n'a demande.
				if (res.cibleIncrement >= 0)
					GrefferIncrement(scratch, doc, res.cibleIncrement, origin, res);
				else
					Graft(scratch, doc, targetParent, origin, res);
				return res;
			}

			// ═══════════════════════════════════════════════════════════════════
			//  L'APERCU — proposer, LAISSER REGARDER, puis poser ou jeter
			// ═══════════════════════════════════════════════════════════════════
			// La garantie 1 de la spec §6.2 : « rien n'est ecrit dans le document
			// tant que l'utilisateur n'a pas valide ». `Ask` ne la tient pas : il
			// pose des l'acceptation. Ces trois fonctions la tiennent PAR ETAT :
			// la proposition validee et rejouee vit DE COTE dans `mPending`, le
			// document de travail n'est touche que par `CommitProposal` — et par
			// la meme porte que la main, comme toujours.
			//
			// ⚠️ `Ask` RESTE : les essais 29/30 de la sonde en dependent, et un
			//    appelant qui veut l'ancien geste « demander = poser » l'a encore.
			//    L'apercu est un chemin EN PLUS, pas un remplacement en douce.

			/// Demander au backend, valider, rejouer — et GARDER DE COTE.
			/// Le document n'est pas touche ; `graftedRoot` reste -1 et
			/// `nodesAdded` 0 tant que rien n'est pose.
			/// La REQUETE exacte, celle que `Propose` et `Ask` envoient.
			/// ⚠️ PUBLIQUE PARCE QUE L'APPEL ASYNCHRONE EN A BESOIN. Un appelant
			///    qui rebatirait l'invite de son cote enverrait autre chose --
			///    c'est exactement la faute que `dXX_invite.txt` a coutee cette
			///    nuit : deux ecritures de la meme regle, et elles divergent.
			void BatirRequete(const char *userAsk, NkUIDocument &doc, NkDesignRequest &req) const {
				BatirInviteComplete(userAsk, req.prompt);
				BuildCatalog(req.catalog, catalogueBref);
				doc.Save(req.currentDoc);
				// La demande part EN DERNIER : voir `NkConverseRequest::demande`.
				req.demande = NkString(userAsk ? userAsk : "");
			}

			/// L'invite TELLE QU'ELLE PART SUR LE FIL, batie par le MEME
			/// constructeur que le dorsal (`EcrireRequete`).
			///
			/// ⚠️ ELLE EXISTE POUR LE CHEMIN ASYNCHRONE, ET VOICI POURQUOI ELLE
			///    EST INDISPENSABLE : `NkTravailIA` pose `req.prompt = invite` et
			///    laisse `catalog` et `currentDoc` VIDES. Lui passer la seule
			///    `BatirInviteComplete` enverrait donc une invite SANS CATALOGUE
			///    ni document courant -- le modele inventerait des noms de
			///    composants, et le rejet « composant inconnu » serait de NOTRE
			///    fait. En lui passant l'invite deja assemblee, `EcrireRequete`
			///    cote dorsal la rend telle quelle : octet pour octet celle du
			///    chemin synchrone.
			NkString InviteAEnvoyer(const char *userAsk, NkUIDocument &doc) const {
				NkDesignRequest req;
				BatirRequete(userAsk, doc, req);
				NkString full;
				NkDesignBackendProcessus::EcrireRequete(req, full);
				return full;
			}

			/// Poser une reponse DEJA OBTENUE comme proposition en attente.
			/// C'est la queue de `Propose`, extraite pour que le chemin asynchrone
			/// l'emprunte AU LIEU d'en ecrire une seconde.
			NkAIResult PoserProposition(const char *replyText, const NkUIDocument *courant = nullptr) {
				NkAIResult res;
				DiscardProposal(); // une proposition chasse l'autre
				mLastReply = NkString(replyText ? replyText : "");
				if (mLastReply.Length() == 0) {
					res.verdict = NkAIVerdict::BackendMuet;
					res.detail = NkString("le dorsal n'a rien rendu");
					return res;
				}
				if (!ValidateReply(mLastReply.Data(), mPending, res, courant))
					return res;
				mPendingCible = res.cibleIncrement; // -1 si document autonome
				mHasPending = true;
				mPendingOrigin = NkString(OrigineCourante());
				res.verdict = NkAIVerdict::Acceptee;
				return res;
			}

			NkAIResult Propose(const char *userAsk, NkUIDocument &doc) {
				NkAIResult res;
				DiscardProposal(); // une proposition chasse l'autre : jamais deux en attente
				// Meme raison qu'en tete de `Ask`, et il FAUT les deux : corriger un
				// seul des deux sites aurait laisse une porte ouverte sur l'autre.
				// *Le meme calcul a deux sites, garde a un seul, est le defaut qu'on
				// rouvre six mois plus tard.*
				mLastReply = NkString("");
				if (!mBackend) {
					res.detail = NkString("aucun backend branche");
					return res;
				}
				NkDesignRequest req;
				BatirRequete(userAsk, doc, req);

				NkDesignReply reply;
				if (!mBackend->Complete(req, reply) || reply.text.Length() == 0) {
					res.verdict = NkAIVerdict::BackendMuet;
					res.detail = reply.error;
					return res;
				}
				// La MEME queue que le chemin asynchrone : une seule ecriture de la
				// regle « valider, rejouer, garder de cote ».
				return PoserProposition(reply.text.Data());
			}

			/// Poser la proposition en attente. C'est ICI que le document change,
			/// et nulle part avant. Sur refus de greffe (cible invalide), la
			/// proposition RESTE en attente : l'utilisateur choisit une autre
			/// cible au lieu de tout redemander.
			NkAIResult CommitProposal(NkUIDocument &doc, int32 targetParent) {
				NkAIResult res;
				if (!mHasPending) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					res.detail = NkString("aucune proposition en attente");
					return res;
				}
				if (!doc.IsValidIndex(targetParent)) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					return res;
				}
				if (mPendingCible >= 0)
					GrefferIncrement(mPending, doc, mPendingCible, mPendingOrigin.Data(), res);
				else
					Graft(mPending, doc, targetParent, mPendingOrigin.Data(), res);
				if (res.Accepted())
					DiscardProposal();
				return res;
			}

			/// Jeter la proposition en attente. Ne touche a rien d'autre.
			void DiscardProposal() {
				mHasPending = false;
				mPendingOrigin = NkString("");
				// ⚠️ ET LA CIBLE AUSSI. Sans cette ligne, une proposition INCREMENT
				//    jetee laisserait sa cible armee, et la proposition SUIVANTE --
				//    un document autonome, qui doit aller sur la selection -- serait
				//    posee sur le noeud de la precedente. C'est la troisieme fois de
				//    la nuit que je croise cet etat rassis (`mLastReply`,
				//    `enTeteCommentaire`, celui-ci) : *un etat qu'on ne remet pas a
				//    zero se fait passer pour une donnee du geste suivant.*
				mPendingCible = -1;
			}

			bool HasProposal() const {
				return mHasPending;
			}
			/// La proposition en attente, pour que l'apercu puisse la METTRE EN
			/// PAGE et la dessiner (`NkComputeLayout` la prend comme n'importe
			/// quel document). Ne vaut que si `HasProposal()`.
			const NkUIDocument &Proposal() const {
				return mPending;
			}

			// ═══════════════════════════════════════════════════════════════════
			//  L'ANNULATION D'UN SEUL GESTE
			// ═══════════════════════════════════════════════════════════════════
			// Il n'existe aujourd'hui AUCUN historique de document dans cette
			// application (mesure du 30/08 : zero occurrence d'Undo/Historique
			// dans Document.h, Panels.h, Canvas.h, main.cpp). En attendant qu'un
			// historique general existe, le retrait d'une greffe IA est possible
			// SANS lui : `RemoveSubtree` retire le sous-arbre entier, et les deux
			// gardes ci-dessous refusent de retirer autre chose que ce qui a ete
			// pose.
			//
			// ⚠️ LES DEUX GARDES NE SONT PAS DU ZELE :
			//   1. l'index peut etre PERIME (`RemoveSubtree` renumerote ; d'autres
			//      suppressions aussi) — on verifie donc que le noeud vise est
			//      bien d'origine IA avant d'y toucher ;
			//   2. si le sous-arbre n'a plus LA MEME TAILLE que ce qui a ete pose,
			//      quelqu'un a greffe ou supprime dedans depuis : retirer « a peu
			//      pres ce qu'on a pose » n'est pas une annulation, c'est une
			//      suppression qui porte le mauvais nom. On refuse, et l'appelant
			//      passe par la suppression normale s'il le veut vraiment.
			static bool Retract(NkUIDocument &doc, const NkAIResult &r) {
				if (!r.Accepted() || !doc.IsValidIndex(r.graftedRoot))
					return false;
				if (doc.nodes[(uint32)r.graftedRoot].prov.author != NkAuthor::IA)
					return false;
				if (SubtreeCount(doc, r.graftedRoot) != r.nodesAdded)
					return false;
				return doc.RemoveSubtree(r.graftedRoot);
			}

			/// Le nombre de noeuds d'un sous-arbre, racine comprise. Sert de garde
			/// a `Retract` ; publique parce qu'une sonde veut la meme mesure.
			static uint32 SubtreeCount(const NkUIDocument &doc, int32 node) {
				if (!doc.IsValidIndex(node))
					return 0;
				uint32 n = 1;
				const NkVector<int32> &kids = doc.nodes[(uint32)node].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					n += SubtreeCount(doc, kids[i]);
				return n;
			}

			/// LE REJEU, isole pour etre reutilisable : un document produit A LA
			/// MAIN se verifie exactement pareil. Rend le nombre de rectangles qui
			/// divergent apres un aller-retour par le texte — 0 = fidele.
			///
			/// ⚠️ ET SA CONDITION D'EXISTENCE : si la relecture ne rend aucun noeud,
			///    il n'y a rien a comparer, et « 0 difference » serait un succes A
			///    VIDE. On rend alors un chiffre non nul, qui vaut echec. C'est le
			///    defaut trouve dans la sonde du 18/08, applique ici avant qu'il se
			///    reproduise.
			static uint32 ReplayDiffs(const NkUIDocument &doc, const NkPaintRect &surface) {
				if (doc.NodeCount() == 0)
					return 1u;
				NkString text;
				doc.Save(text);
				NkUIDocument again;
				if (!again.Load(text.Data()))
					return 1u;
				if (again.NodeCount() != doc.NodeCount())
					return 1u;

				NkLayoutResult a, b;
				NkComputeLayout(doc, surface, a);
				NkComputeLayout(again, surface, b);
				if (a.ValidCount() == 0 || a.ValidCount() != b.ValidCount())
					return 1u;
				uint32 diffs = 0;
				for (uint32 i = 0; i < (uint32)doc.NodeCount(); ++i) {
					const bool ha = a.Has((int32)i), hb = b.Has((int32)i);
					if (ha != hb) {
						++diffs;
						continue;
					}
					if (!ha)
						continue;
					if (!SameRect(a.At((int32)i), b.At((int32)i)))
						++diffs;
				}
				return diffs;
			}

			static bool SameRect(const NkPaintRect &a, const NkPaintRect &b) {
				return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.w, b.w) && Near(a.h, b.h);
			}

			const NkString &LastReply() const {
				return mLastReply;
			}

		private:
			// ── LA VALIDATION, UNE SEULE FOIS ───────────────────────────────
			// `Apply` (demander = poser) et `Propose` (demander = garder de cote)
			// jugent une reponse EXACTEMENT pareil. Deux copies de ce jugement
			// auraient diverge au premier verdict ajoute — et une reponse
			// acceptee en apercu puis refusee a la pose serait exactement le
			// genre de defaut qu'on ne relie pas a sa cause.
			//
			// Rend vrai si `scratch` porte un document charge, connu du registre
			// et fidele au rejeu ; sinon remplit `res` avec la raison du refus.
			// ═══════════════════════════════════════════════════════════════════
			//  L'INCREMENT — un DELTA sur le document ouvert
			// ═══════════════════════════════════════════════════════════════════
			//  ⚠️ LES QUATRE GARDES SONT CELLES DU DOCUMENT AUTONOME, APPLIQUEES A
			//     UN DELTA. Mesurees sur les 16 increments du 20/09 :
			//
			//       (1) toute reference `parent` existe ou est creee ici   0/16 violent
			//       (2) aucun orphelin, aucune seconde racine              0/16 violent
			//       (3) aucun numero cree ne heurte un noeud existant      6/16 VIOLENT
			//       (4) un seul point d'accrochage                        11/16 tiennent
			//
			//  🔴 (3) EST LA PLUS IMPORTANTE, ET CE N'EST PAS « deux fois le meme
			//     numero ». 4 increments sur 16 CREENT un numero **et s'en servent
			//     comme parent**, alors qu'il existe deja : « parent = 28 » designe
			//     alors le noeud 28 du document, ou celui qu'on vient de creer --
			//     **indecidable**. Trancher au jugé poserait le bouton sous le
			//     mauvais parent une fois sur quatre, en silence, et ca ressemblerait
			//     a un caprice du modele.
			//
			//     La regle qui rend le cas IRREPRESENTABLE (meme doctrine que
			//     `parent` contre `enfants`) : *un increment ne numerote qu'AU-DESSUS
			//     du plus grand noeud existant.* 10/16 le font deja naturellement ;
			//     la regle ne contrarie presque personne, elle ferme une porte.
			//
			//  ⚠️ (4) N'EST PAS ELARGIE. 5/16 accrochent en PLUSIEURS points : c'est
			//     une capacite reellement neuve, et elle est REFUSEE AVEC SON MOTIF.
			//     *Un increment applique aux trois quarts serait pire que refuse.*
			/// Decoupe un increment en blocs. Rend, pour chaque `noeud N` : son
			/// numero, son `parent` (-1 s'il n'en a pas), et le RESTE du bloc
			/// VERBATIM -- lignes `noeud` et `parent` retirees, tout le reste garde.
			///
			/// ⚠️ `[ \t]` ET JAMAIS `\s` DANS UN MOTIF DE LIGNE. Mon extracteur de
			///    squelette a rendu « composant = largeur = expand 0 0 0 » ce matin
			///    parce que `\s*` apres le `=` traverse le saut de ligne et avale la
			///    ligne suivante. Ici on lit caractere par caractere, sans motif.
			static bool DecouperIncrement(const char *texte, NkVector<int32> &numeros,
										  NkVector<int32> &parents, NkVector<NkString> &corps) {
				numeros.Clear();
				parents.Clear();
				corps.Clear();
				if (!texte)
					return false;
				const char *p = texte;
				bool dansBloc = false;
				while (*p) {
					const char *fin = p;
					while (*fin && *fin != '\n')
						++fin;
					// la ligne, sans le retour chariot de Windows
					char ligne[512];
					uint32 k = 0;
					for (const char *q = p; q < fin && k + 1 < sizeof(ligne); ++q)
						if (*q != '\r')
							ligne[k++] = *q;
					ligne[k] = 0;
					const char *s = ligne;
					while (*s == ' ' || *s == '\t')
						++s;
					if (CommencePar2(s, "noeud ")) {
						const int32 num = LireEntier(s + 6);
						if (num < 0)
							return false;
						numeros.PushBack(num);
						parents.PushBack(-1);
						corps.PushBack(NkString(""));
						dansBloc = true;
					} else if (dansBloc && CommencePar2(s, "parent")) {
						const char *v = s + 6;
						while (*v == ' ' || *v == '\t' || *v == '=')
							++v;
						parents[(uint32)parents.Size() - 1] = LireEntier(v);
					} else if (dansBloc && k > 0) {
						NkString &c = corps[(uint32)corps.Size() - 1];
						c.Append("  ");
						c.Append(s);
						c.Append("\n");
					}
					p = *fin ? fin + 1 : fin;
				}
				return numeros.Size() > 0;
			}

			static bool CommencePar2(const char *s, const char *p) {
				for (uint32 i = 0; p[i]; ++i)
					if (s[i] != p[i])
						return false;
				return true;
			}
			/// ⚠️ PAS `atoi` : il rend 0 sur « rien », et 0 est l'indice de la
			///    RACINE. Une absence lue comme « racine » reparenterait tout.
			static int32 LireEntier(const char *s) {
				while (*s == ' ' || *s == '\t')
					++s;
				if (*s < '0' || *s > '9')
					return -1;
				int32 v = 0;
				while (*s >= '0' && *s <= '9')
					v = v * 10 + (int32)(*s++ - '0');
				return v;
			}

			bool ValiderIncrement(const char *texte, const NkUIDocument &courant,
								  NkUIDocument &scratch, NkAIResult &res) {
				res.verdict = NkAIVerdict::TexteNonConforme;
				const int32 nbCourant = (int32)courant.NodeCount();
				if (nbCourant <= 0) {
					res.detail = NkString("aucun document ouvert : il n'y a rien a completer");
					return false;
				}
				// ── LIRE LES BLOCS `noeud N` ────────────────────────────────
				NkVector<int32> numeros, parents;
				NkVector<NkString> corps;
				if (!DecouperIncrement(texte, numeros, parents, corps) || numeros.Size() == 0) {
					res.detail = NkString(
						"le modele a repondu en phrases au lieu de dessiner une "
						"interface. Essayez de decrire les ZONES de l'ecran "
						"(« a gauche… au centre… en bas… »), ou decoupez la demande "
						"en deux ecrans plus simples, puis reessayez.");
					return false;
				}
				const uint32 n = (uint32)numeros.Size();

				// ── (3) LA NUMEROTATION, ET C'EST ELLE QUI FERME L'INDECIDABLE ──
				for (uint32 i = 0; i < n; ++i)
					if (numeros[i] < nbCourant) {
						char b[224];
						snprintf(b, sizeof(b),
								 "la reponse cree un noeud %d alors que le document en "
								 "compte deja %d : impossible de savoir si elle veut le "
								 "noeud existant ou un nouveau. Rien n'a ete pose.",
								 (int)numeros[i], (int)nbCourant);
						res.detail = NkString(b);
						return false;
					}
				for (uint32 i = 0; i < n; ++i)
					for (uint32 j = i + 1; j < n; ++j)
						if (numeros[i] == numeros[j]) {
							res.detail = NkString("la reponse cree deux fois le meme numero "
												  "de noeud. Rien n'a ete pose.");
							return false;
						}

				// ── (1) ET (4) : OU CA S'ACCROCHE ───────────────────────────
				int32 cible = -1;
				for (uint32 i = 0; i < n; ++i) {
					const int32 p = parents[i];
					if (p < 0) {
						res.detail = NkString("un noeud de la reponse ne dit pas a quoi il "
											  "se rattache. Rien n'a ete pose.");
						return false;
					}
					if (p < nbCourant) { // un noeud du document ouvert
						if (cible < 0)
							cible = p;
						else if (cible != p) {
							char b[224];
							snprintf(b, sizeof(b),
									 "la reponse se rattache a DEUX endroits du document "
									 "(noeuds %d et %d). L'outil ne sait poser qu'a un seul "
									 "endroit a la fois : rien n'a ete pose.",
									 (int)cible, (int)p);
							res.detail = NkString(b);
							return false;
						}
						continue;
					}
					bool creeIci = false; // sinon, ce doit etre un noeud de l'increment
					for (uint32 k = 0; k < n && !creeIci; ++k)
						creeIci = (numeros[k] == p);
					if (!creeIci) {
						char b[192];
						snprintf(b, sizeof(b),
								 "la reponse se rattache a un noeud %d qui n'existe nulle "
								 "part. Rien n'a ete pose.", (int)p);
						res.detail = NkString(b);
						return false;
					}
				}
				if (cible < 0) {
					res.detail = NkString("la reponse ne se rattache a aucun noeud du document "
										  "ouvert. Rien n'a ete pose.");
					return false;
				}

				// ── RENUMEROTER VERS UN DOCUMENT DE COTE ────────────────────
				// ⚠️ ON NE REECRIT QUE LES DEUX LIGNES QU'ON CONTROLE (`noeud` et
				//    `parent`) : tout le reste du bloc part VERBATIM. Reserialiser
				//    le noeud ici serait une seconde ecriture du format, et elle
				//    divergerait au premier champ ajoute.
				// Le noeud 0 du document de cote est une RACINE JETABLE : elle n'est
				// jamais greffee, ce sont ses enfants qu'on pose (voir `GrefferIncrement`).
				NkString t("nkuidoc 1\ntitre = Increment\nnoeud 0\n  libelle = Increment\n"
						   "  composant = \n  largeur = expand 1 0 0\n  hauteur = expand 1 0 0\n");
				for (uint32 i = 0; i < n; ++i) {
					char e[48];
					snprintf(e, sizeof(e), "noeud %u\n", (unsigned)(i + 1));
					t.Append(e);
					const int32 p = parents[i];
					int32 pLocal = 0; // externe -> la racine jetable
					if (p >= nbCourant)
						for (uint32 k = 0; k < n; ++k)
							if (numeros[k] == p)
								pLocal = (int32)(k + 1);
					snprintf(e, sizeof(e), "  parent = %d\n", (int)pLocal);
					t.Append(e);
					t.Append(corps[i]);
				}

				uint32 inconnus = 0;
				if (!scratch.Load(t.CStr(), &inconnus) || scratch.NodeCount() == 0) {
					res.detail = NkString("la reponse ne se relit pas comme un ajout coherent. "
										  "Rien n'a ete pose.");
					return false;
				}
				res.unknownComponents = inconnus;
				if (inconnus > 0 || !NkUIDocument::CanGraft(scratch, 0)) {
					res.verdict = NkAIVerdict::ComposantInconnu;
					return false;
				}
				res.cibleIncrement = cible;
				res.verdict = NkAIVerdict::Acceptee;
				return true;
			}

			bool ValidateReply(const char *replyText, NkUIDocument &scratch, NkAIResult &res,
							   const NkUIDocument *courant = nullptr) {
				// 1. Extraire le document de la reponse. Un modele encadre
				//    volontiers sa sortie de commentaires ou de balises : on part
				//    de la premiere ligne `nkuidoc`. Si elle n'y est pas, c'est
				//    non — on ne devine pas ce qu'il a voulu dire.
				const char *body = FindHeader(replyText);
				// ⚠️ L'INCREMENT : LE MODELE FAIT LA CHOSE JUSTE, ET ON LA REFUSAIT.
				//    Mesure du 20/09, 30 demandes sur le document de 42 noeuds de
				//    Rodolf : sur 16 reponses rendues, **16 sont des INCREMENTS** --
				//    des lignes `noeud`, aucun en-tete `nkuidoc`. Zero prose.
				//    Demander « ajoute un bouton » a un modele qui voit le document
				//    produit un DELTA, pas un document autonome : c'est le bon
				//    comportement, et notre validateur l'appelait « pas de document
				//    lisible ».
				if (!body && courant)
					return ValiderIncrement(replyText, *courant, scratch, res);
				if (!body) {
					// ⚠️ LE MESSAGE PARLAIT DE NOTRE FORMAT, PAS DE CE QU'IL PEUT
					//    FAIRE. « pas de ligne `nkuidoc` » nomme un detail interne
					//    que l'utilisateur n'a aucune raison de connaitre, et ne lui
					//    dit rien de son prochain geste. Rodolf l'a recu le 20/09 et
					//    n'a eu aucune piste. *Un refus qui n'ouvre aucune porte est
					//    un refus qui n'aide pas.*
					//
					//    ⚠️ ET IL DISTINGUE DEUX CHOSES QUE L'ANCIEN CONFONDAIT :
					//    « le modele a repondu en francais au lieu d'un document »
					//    et « le modele n'a rien dit ». Elles se reparent a deux
					//    endroits differents.
					res.verdict = NkAIVerdict::TexteNonConforme;
					uint32 nonBlanc = 0;
					for (const char *p = replyText; p && *p; ++p)
						if (*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t')
							++nonBlanc;
					if (nonBlanc == 0) {
						res.detail = NkString(
							"le modele n'a rien ecrit. Reessayez : la meme demande "
							"aboutit souvent au second essai.");
					} else {
						res.detail = NkString(
							"le modele a repondu en phrases au lieu de dessiner une "
							"interface. Essayez de decrire les ZONES de l'ecran "
							"(« a gauche… au centre… en bas… »), ou decoupez la demande "
							"en deux ecrans plus simples, puis reessayez.");
					}
					return false;
				}

				// 2. Charger de cote.
				uint32 unknown = 0;
				if (!scratch.Load(body, &unknown) || scratch.NodeCount() == 0) {
					res.verdict = NkAIVerdict::TexteNonConforme;
					res.detail = NkString("document illisible ou structure incoherente");
					return false;
				}
				// ⚠️ UN LECTEUR QUI ACCEPTE EN PERDANT EST PIRE QU'UN LECTEUR QUI
				//    REFUSE : il eteint l'alarme en meme temps qu'il cause le
				//    dommage. Mesure du 20/09 : le MEME document a trois noeuds,
				//    avec `noeud 0` ecrit EN DERNIER, etait ACCEPTE et ne posait
				//    qu'UN noeud sur trois. Sans un mot.
				//
				//    La cause est en amont, et elle est nette : le chargeur n'a
				//    JAMAIS lu le numero ecrit apres `noeud` -- l'indice vient de
				//    la POSITION dans le fichier. Un document dont les numeros ne
				//    suivent pas leur position est donc relu comme un AUTRE
				//    document, et la reconstruction des liens y fabrique un noeud
				//    son propre parent.
				//
				//    Cette garde ne repare pas cette cause : elle rend impossible
				//    qu'elle passe en silence. *Les deux comptes sont dans le
				//    motif, parce qu'un refus qui ne dit pas de combien il manque
				//    envoie chercher au mauvais endroit.*
				const uint32 lus = scratch.NodeCount();
				const uint32 atteignables = SubtreeCount(scratch, 0);
				if (atteignables != lus) {
					res.verdict = NkAIVerdict::TexteNonConforme;
					char b[224];
					snprintf(b, sizeof(b),
							 "le document declare %u noeud(s), %u seulement sont relies a la "
							 "racine : %u seraient PERDUS en silence. Refuse. (les numeros "
							 "`noeud N` doivent suivre l'ordre d'ecriture)",
							 (unsigned)lus, (unsigned)atteignables,
							 (unsigned)(lus - atteignables));
					res.detail = NkString(b);
					return false;
				}
				res.unknownComponents = unknown;
				if (unknown > 0 || !NkUIDocument::CanGraft(scratch, 0)) {
					res.verdict = NkAIVerdict::ComposantInconnu;
					return false;
				}

				// 3. LE REJEU. C'est le verificateur, et il est mecanique.
				res.replayDiffs = ReplayDiffs(scratch, replaySurface);
				if (res.replayDiffs > 0) {
					res.verdict = NkAIVerdict::RejeuDivergent;
					return false;
				}
				return true;
			}

			// La porte — la MEME que la main — puis le tampon « rejouee » : il
			// est pose parce que le rejeu a EU LIEU dans `ValidateReply`, pas
			// parce que ca vient de l'IA.
			/// Poser un INCREMENT : ce ne sont PAS le sous-arbre de la racine
			/// jetable qu'on greffe, ce sont SES ENFANTS, un par un, sur la cible
			/// que la reponse a nommee.
			///
			/// ⚠️ TOUT OU RIEN, ET C'EST VERIFIE AVANT DE TOUCHER AU DOCUMENT.
			///    `GraftFrom` verifie deja chaque sous-arbre, mais une boucle qui
			///    verifierait au fil de l'eau poserait les deux premiers noeuds
			///    puis refuserait le troisieme -- *« une greffe a moitie faite
			///    laisserait un document que personne n'a voulu »*, et la promesse
			///    « une seule operation annulable » de la carte deviendrait fausse
			///    exactement quand elle compte.
			void GrefferIncrement(const NkUIDocument &scratch, NkUIDocument &doc, int32 cible,
								  const char *origin, NkAIResult &res) {
				if (!scratch.IsValidIndex(0) || !doc.IsValidIndex(cible)) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					return;
				}
				const NkVector<int32> &tetes = scratch.nodes[0].children;
				for (uint32 i = 0; i < (uint32)tetes.Size(); ++i)
					if (!NkUIDocument::CanGraft(scratch, tetes[i])) {
						res.verdict = NkAIVerdict::ComposantInconnu;
						return;
					}
				const uint32 before = doc.NodeCount();
				int32 premier = -1;
				for (uint32 i = 0; i < (uint32)tetes.Size(); ++i) {
					const int32 r = doc.GraftFrom(scratch, tetes[i], cible, true, NkAuthor::IA,
												  origin);
					if (r < 0) { // ne doit pas arriver : tout a ete verifie au-dessus
						res.verdict = NkAIVerdict::GreffeRefusee;
						return;
					}
					doc.MarkVerified(r);
					if (premier < 0)
						premier = r;
				}
				res.verdict = NkAIVerdict::Acceptee;
				res.graftedRoot = premier;
				res.nodesAdded = doc.NodeCount() - before;
			}

			void Graft(const NkUIDocument &scratch, NkUIDocument &doc, int32 targetParent,
					   const char *origin, NkAIResult &res) {
				const uint32 before = doc.NodeCount();
				const int32 root = doc.GraftFrom(scratch, 0, targetParent, true, NkAuthor::IA, origin);
				if (root < 0) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					return;
				}
				doc.MarkVerified(root);
				res.verdict = NkAIVerdict::Acceptee;
				res.graftedRoot = root;
				res.nodesAdded = doc.NodeCount() - before;
			}

			static bool Near(float32 a, float32 b) {
				const float32 d = a - b;
				return d < 0.01f && d > -0.01f;
			}
			/// La premiere ligne qui commence par `nkuidoc`, en debut de ligne.
			static const char *FindHeader(const char *s) {
				if (!s)
					return nullptr;
				const char *line = s;
				while (*line) {
					const char *p = line;
					while (*p == ' ' || *p == '\t')
						++p;
					if (p[0] == 'n' && p[1] == 'k' && p[2] == 'u' && p[3] == 'i' && p[4] == 'd' &&
						p[5] == 'o' && p[6] == 'c')
						return p;
					while (*line && *line != '\n')
						++line;
					if (*line)
						++line;
				}
				return nullptr;
			}
			// Les listes de valeurs du prompt sont engendrees depuis les MEMES
			// fonctions de nom que l'ecrivain de fichier. Les taper a la main ferait
			// deux verites, et la seconde vieillirait en silence : le modele
			// recevrait un vocabulaire perime et ses reponses seraient rejetees sans
			// qu'on comprenne pourquoi.
			static void AppendModes(NkString &out) {
				for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i) {
					if (i)
						out.Append(" | ");
					out.Append(NkSizeModeName((NkSizeMode)i));
				}
			}
			static void AppendKinds(NkString &out) {
				for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i) {
					if (i)
						out.Append(" | ");
					out.Append(NkLayoutKindName((NkLayoutKind)i));
				}
			}
			static void AppendAligns(NkString &out) {
				for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i) {
					if (i)
						out.Append(" | ");
					out.Append(NkAlignName((NkAlign)i));
				}
			}

			NkIDesignBackend *mBackend = nullptr;
			NkString mLastReply;

			// La proposition en attente d'un `CommitProposal`. Un document
			// entier, pas un texte : ce qui a ete valide est ce qui sera pose,
			// sans repasser par une analyse qui pourrait juger autrement.
			NkUIDocument mPending;
			NkString mPendingOrigin;
			bool mHasPending = false;
			/// Le point d'accrochage d'un increment en attente, -1 si la proposition
			/// est un document autonome. Il voyage avec `mPending` : le perdre ferait
			/// poser a la selection, c'est-a-dire ailleurs que ce que la reponse dit.
			int32 mPendingCible = -1;
	};

} // namespace nkuidesign
