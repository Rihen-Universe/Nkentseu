#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerAiPanel.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  (b9) L'ASSISTANT — LA FORME TRANCHEE PAR RODOLF LE 17/09
// =============================================================================
//  « que ce soit pour nkcode, nk3dmodeler ou nkuidesign ou les futures, ils
//    doivent ressembler a celui de VSCode, sauf si tu as mieux a proposer »
//  La forme est ecrite dans `echanges/PANNEAU_IA_SPEC.md`.
//
//  ⚠️ 21/09 : LE PANNEAU EST DESORMAIS CELUI DU KIT (`NKEditorKit/NkAiPanneau.h`),
//     le meme que NKUIDesign et NKCode. PLUS D'ONGLETS (§6 de la spec) : le
//     fournisseur puis le modele se choisissent dans la pastille de la barre du
//     bas. Ce fichier ne garde que ce que le modeleur est seul a savoir. Les
//     paragraphes ci-dessous racontent l'histoire ; le bloc « 21/09 » plus bas
//     dit l'etat.
//
//  ⚠️ CE FICHIER EST REECRIT, ET LA RAISON EST UN DEFAUT, PAS UN GOUT.
//     La premiere version (17/09, 18h09) peignait le panneau DANS la pastille
//     de proprietes -- c'est-a-dire a l'interieur d'un panneau hote. C'est
//     exactement le piege que la specification interdit : deux menus de
//     NKUIDesign dessines ainsi laissaient passer les clics une image sur deux.
//     Le panneau vit desormais sur la COUCHE OVERLAY (`NkOvPainter()`), sur sa
//     propre couche de registre.
//
//  CE QUI N'A PAS CHANGE, ET QUI EST L'ESSENTIEL :
//    - `NkAiSoumettre` reste LE POINT D'ENTREE UNIQUE. Le bouton l'appelle, et
//      le crochet de mesure `NK_AI_DEMANDE` entre exactement la.
//    - Le panneau N'APPELLE PAS LE PONT. Il ecrit dans `st.aiPending` ; la
//      boucle execute. Un panneau qui appellerait `NkVpPoserAction` lui-meme
//      serait un second chemin vers le meme etat.
//
//  CE QUE LA FORME AJOUTE, ET QUI NE VIENT PAS DE LA CAPTURE :
//    1. CHAQUE OPERATION PORTE SON EFFET **MESURE** ET SON ANNULATION. La ligne
//       dit « faces 6 -> 384 », lu dans les compteurs de l'hote avant et apres.
//    2. LE REFUS EST UN BLOC A PART, avec son motif. C'est une reponse, pas une
//       panne : une IA invente des verbes, c'est le cas normal.
//    3. LA LIGNE D'ETAT DIT LE DORSAL ET SON COUT -- et elle dit **non charge**
//       tant qu'il ne l'est pas. Afficher « 4 444 Mo » comme si le modele
//       occupait la carte serait faux aujourd'hui, et ce genre de chiffre finit
//       par etre cru.
//
//  CE QUE CE PANNEAU EST DEPUIS LE 19/09 : une phrase francaise y entre, un
//  verbe du contrat en sort. `NkModelerIA.h` parle a NKConverse en asynchrone
//  et main.cpp recolte a chaque image. Mesure : « subdivise le cube deux fois »
//  -> `subdivide:2`, 0,91 s a chaud et 10,35 s A FROID -- avec 1478 images
//  passees pendant l attente, donc sans gel.
//
//  ⚠️ CE PARAGRAPHE DISAIT « rien n est branche » JUSQU AU 20/09, ET C ETAIT
//     FAUX DEPUIS LA VEILLE. Un commentaire perime ne vieillit pas comme un
//     chiffre : il se lit comme une CONSIGNE, et le lecteur suivant renonce a
//     chercher ce qui existe. Il avait deja failli faire reecrire ce branchement.
//
//  ⚠️ CE QUE LE PANNEAU N EST TOUJOURS PAS : une CONVERSATION. Un tour, une
//     demande, un verbe -- aucune memoire d un echange a l autre. Le fil ne
//     porte donc AUCUN bloc de « reflexion » : en fabriquer un serait du
//     theatre, personne ne pense derriere.
//
//  UNE DETTE NOMMEE, AVEC SA CONDITION DE RETRAIT (la chasse fixe est levee le
//  21/09 : main.cpp charge Cousine et la televerse, `NkAiPoliceMono`) :
//    - La specification demande que le panneau soit monte depuis un document
//      `.nkgui`. Le mecanisme de zone hote vient d'arriver par transit ; le
//      panneau doit d'abord exister et se mesurer. Le jour venu, seule
//      l'ORIGINE des rectangles change, pas ce qu'ils montrent.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkAiPanneau.h" // LE panneau IA, commun aux applications (21/09)
#include "NKEditorKit/Components/NkGuiComponentPaint.h" // le peintre NKGui : polices grasse et fixe
#include "NK3DModeler/Shell/NkModelerContrat.h" // le vocabulaire des verbes (le « / »)
#include "NKTime/NkChrono.h" // la duree de la conversation

// ⚠️ IL INCLUT CE QU'IL UTILISE. `snprintf` arrivait ici PAR CHANCE, tire par un
//    en-tete voisin ; la recolte ajoute `fopen`/`fputs`/`FILE`, et compter sur
//    la meme chance serait la faute exacte qui a coute 20 erreurs a NKPA -- un
//    en-tete qui compilait chez ses deux consommateurs d'alors, et cassait chez
//    le premier qui ne tirait pas sa dependance.
#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		// Copie bornee, sans dependre de strncpy : le depot ecrit ses propres
		// primitives de chaine (zero-STL), et une troncature doit etre VOULUE.
		inline void NkAiCopie(char *dst, uint32 cap, const char *src) {
			if (!dst || cap == 0u)
				return;
			uint32 i = 0;
			if (src)
				for (; src[i] && i + 1u < cap; ++i)
					dst[i] = src[i];
			dst[i] = 0;
		}

		// ── LA RECOLTE DES DEMANDES REELLES ───────────────────────────────────
		// ⚠️ POURQUOI ELLE EXISTE. On a voulu savoir combien des demandes de
		//    Rodolf portent une condition CHIFFREE (« subdivise jusqu'a 1000
		//    faces ») et combien n'en portent pas (« rends ca plus beau ») --
		//    parce que la reponse decide s'il faut construire une boucle d'agent.
		//    Il n'existait AUCUN corpus : les seules demandes qu'on ait trouvees
		//    dans les journaux etaient celles des bancs de mesure. Fabriquer
		//    vingt phrases plausibles aurait mesure NOTRE imagination de son
		//    usage, pas son usage. On recolte donc les vraies.
		//
		// ⚠️ ELLE RECOLTE, ELLE NE JUGE PAS. Aucune classification, aucun
		//    comptage, aucun verdict : la phrase BRUTE, une par ligne. Classer a
		//    la volee figerait aujourd'hui les categories qu'on cherche
		//    justement a decouvrir, et un corpus deja trie ne peut plus rien
		//    refuter.
		//
		// ⚠️ ET RIEN NE QUITTE LA MACHINE. Ce fichier est un journal LOCAL, a
		//    cote des autres, dans `logs/`. Il n'est envoye nulle part, ni au
		//    dorsal local ni au dorsal distant. Ce sont les phrases de Rodolf :
		//    elles lui appartiennent, et il doit pouvoir les lire et les effacer
		//    d'un geste -- c'est un fichier texte.
		//
		// ⚠️ MODE « a » ET NON « w ». Ouvrir en ecriture TRONQUE des l'ouverture :
		//    le premier lancement aurait efface la recolte de la veille.
		inline void NkAiRecolter(const char *phrase) {
			if (!phrase || !phrase[0])
				return;
			std::FILE *f = std::fopen("logs/nk3dmodeler_demandes.txt", "a");
			if (!f)
				return; // pas de `logs/` : on se tait, une recolte n'est pas critique
			std::fputs(phrase, f);
			std::fputc('\n', f);
			std::fclose(f);
		}

		inline bool NkAiVide(const char *s) {
			if (!s)
				return true;
			for (uint32 i = 0; s[i]; ++i)
				if (s[i] != ' ' && s[i] != '\t')
					return false; // au moins un caractere qui n'est pas du blanc
			return true;
		}

		// ── LES FOURNISSEURS ───────────────────────────────────────────────────
		// LE LOCAL EST LE PREMIER ET LE DEFAUT. Les deux autres existent dans la
		// barre parce qu'ils existent dans la maison (PV3DE parle a Claude et a
		// Ollama), mais rien ne les branche AU MODELEUR : leur onglet le DIT et
		// eteint le composeur, au lieu d'envoyer au pont un texte en laissant
		// croire qu'un modele l'a produit.
		inline const char *NkAiFournisseur(int32 i) {
			switch (i) {
				case 0:
					return "Local";
				case 1:
					return "Claude";
				case 2:
					return "Ollama";
				default:
					return "?";
			}
		}
		static const int32 kAiFournisseurs = 3;

		// ── LE FIL : POSER UN BLOC ─────────────────────────────
		// ⚠️ REND UN IDENTIFIANT, PLUS UN INDICE. C'est tout le lot : la
		//    fenetre glisse, donc une position ne designe rien de stable.
		//    0 = le bloc n'est PAS entre (le fil l'a refuse, avec son motif).
		//
		//    Les quatre lignes qui rattrapaient `st.aiEnCours` a la main ont
		//    disparu d'ici : un identifiant ne glisse pas. On ne propage pas une
		//    correction au site qui l'avait oubliee, on supprime sa raison d'etre.
		inline uint32 NkAiPousser(NkModelerState &st, NkModelerState::AiType t,
			   const char *ligne) {
			// Les capacites sont posees ICI, a chaque appel : c'est idempotent et ca
			// evite un second endroit ou l'on pourrait oublier de les declarer. Le
			// fil refuse tout ce qui n'est pas declare, donc un oubli donnerait un
			// fil vide -- visible en une seconde, mais autant ne pas l'ecrire.
			editorkit::NkAiCapacites cap = editorkit::NkAiCapacites::Texte();
			cap.produitOutil = true; // une operation porte une entree et une sortie
			cap.produitRefus = true; // un verbe inconnu est une REPONSE, pas une panne
			st.aiFil.Declarer(cap);
			// ⚠️ LA FENETRE RESTE A 16, ET IL A FALLU LE DIRE. Le fil du kit a un
			//    plafond par defaut de 200 ; la migration aurait donc fait passer ce
			//    panneau de 16 a 200 blocs EN SILENCE. Le lot dit « tout ce qui n'est
			//    pas le fil se comporte exactement comme avant » -- la capacite en
			//    fait partie.
			//    C'est le banc qui l'a montre : A3 verdissait sans que rien ne glisse
			//    (« indice 10 avant, 10 apres »), donc sans rien eprouver. *Un essai
			//    vert dont la condition n'est plus reunie ne mesure plus rien.*
			st.aiFil.PoserPlafond(16);
			
			editorkit::NkAiBlocDonnees b;
			switch (t) {
				case NkModelerState::AiType::Demande:
					b.type = editorkit::NkAiBloc::Demande;
					b.texte = NkString(ligne);
					break;
				case NkModelerState::AiType::Operation:
					// ⚠️ LE TITRE EST LE VERBE, et il est REEL : `NkAiTour` recoit
					//    `dem`, qui porte deja la forme du contrat (`subdivide:2`) --
					//    la traduction a eu lieu en amont. Aucun libelle invente.
					b.type = editorkit::NkAiBloc::Outil;
					b.titre = NkString(ligne);
					b.entree = NkString(ligne);
					// L'effet n'est pas encore mesure, et le dire est plus honnete qu'un
					// blanc : c'est deja ce que le panneau affichait.
					b.effet = NkString("effet en cours de mesure");
					break;
				case NkModelerState::AiType::Refus:
					b.type = editorkit::NkAiBloc::Refus;
					b.motif = NkString(ligne);
					break;
				default: // Note
					b.type = editorkit::NkAiBloc::Prose;
					b.texte = NkString(ligne);
					break;
			}
			NkString pourquoi;
			if (!st.aiFil.Pousser(b, pourquoi)) {
				// Le fil refuse AVEC SON MOTIF. On ne l'avale pas : un bloc qui
				// n'entre pas sans que personne ne le sache est un trou dans le fil.
				NkAiCopie(st.aiMotif, sizeof(st.aiMotif), pourquoi.CStr());
				return 0u;
			}
			return st.aiFil.At(st.aiFil.Taille() - 1).id;
		}

		// ── LA SOUMISSION, ET ELLE EST LA SEULE ────────────────────────────────
		// Le bouton l'appelle, la touche Entree l'appellera, et le crochet de
		// mesure `NK_AI_DEMANDE` entre EXACTEMENT ICI. Ce n'est pas un raccourci
		// pour la sonde : c'est le meme point d'entree, sinon la sonde mesurerait
		// un chemin que Rodolf n'emprunte jamais.
		inline bool NkAiSoumettre(NkModelerState &st, const char *texte) {
			// ── PAS D'ASSISTANT AU LANCEUR (Rodolf, 17/09) ─────────────────────
			// « le chat s'ouvre seulement quand on est EN PROJET, et non depuis le
			// lanceur ». La raison tient en une phrase : l'IA AGIT SUR QUELQUE
			// CHOSE. Sur l'ecran d'accueil il n'y a pas de maillage, donc aucune
			// de ses actions n'a de cible, et la moitie utile du panneau -- l'effet
			// mesure, l'annulation -- n'a plus de sens.
			//
			// ⚠️ ON REFUSE EN LE DISANT, on n'ouvre pas une coquille vide. Un
			//    panneau present mais inerte apprend a l'utilisateur que cette
			//    pastille ne sert a rien, et il ne l'ouvrira plus le jour ou elle
			//    servira.
			if (st.welcome) {
				NkAiCopie(st.aiMotif, sizeof(st.aiMotif),
						  "Aucun projet ouvert : l'assistant agit sur un maillage.");
				st.aiMotifEstRefus = true;
				st.aiOuvert = false; // et surtout : il ne s'ouvre pas
				return false;
			}
			if (NkAiVide(texte)) {
				// LE ZERO. On ne soumet rien, et on DIT pourquoi : un bouton qui
				// ne reagit pas se lit comme un bouton casse.
				NkAiCopie(st.aiMotif, sizeof(st.aiMotif),
						  "Rien a faire : la demande est vide.");
				st.aiMotifEstRefus = true;
				return false;
			}
			NkAiCopie(st.aiPending, sizeof(st.aiPending), texte);
			// LE SUJET EST LA PREMIERE DEMANDE, comme dans la capture -- le kit le lit
			// dans le fil (`NkAiPanneau::Sujet`) : plus de copie a tenir a jour.
			(void)NkAiPousser(st, NkModelerState::AiType::Demande, texte);
			// ── UNE DEMANDE **MARQUE** LE PANNEAU, ELLE NE L'OUVRE PLUS ───────
			// L'ancienne version posait ici `st.aiOuvert = true`, pour une raison qui
			// reste vraie : une demande soumise dans un panneau ferme repondrait sans
			// que personne ne voie la reponse.
			// 
			// Ce que cette raison ne justifiait pas, c'est de PRENDRE L'ECRAN. Rodolf
			// a demande que l'assistant soit « une pastille comme les autres » -- et
			// une pastille ne s'ouvre pas toute seule. *Ouvrir de force repond au
			// besoin de l'APPLICATION ; marquer repond a celui de l'UTILISATEUR.*
			// 
			// `NkAiPousser` vient de poser un bloc : la marque EXISTE DEJA, elle est
			// `st.aiFil.NonVus() > 0`, et la pastille la peint. Il n'y a donc rien a
			// poser ici, et surtout pas un second etat qui dirait la meme chose.
			// 
			// ⚠ ET LE BANC N'Y PERD RIEN, VERIFIE EN LISANT LES DEUX CROCHETS :
			//   `NK_AI_DEMANDE` ne lit pas `aiOuvert` -- il lit le retour de cette
			//   fonction et `st.aiMotif` ; `NK_AI_TRACE` imprime desormais la MARQUE
			//   meme panneau ferme, et `NK_AI_PANNEAU` ouvre pour qui veut mesurer
			//   les rectangles. C'est ce qui autorise a ne garder QU'UN comportement,
			//   au lieu d'un pour l'humain et un pour l'instrument.
			return true;
		}

		// ── LA TABLE DE MESURE : trouver, poser, retirer ─────────────
		// ⚠️ TOUT PASSE PAR L'IDENTIFIANT. Une table annexe clee par la position
		//    reintroduirait exactement le defaut que ce lot retire.
		inline NkModelerState::AiMesure *NkAiMesureDe(NkModelerState &st, uint32 id) {
			if (id == 0u) return nullptr;
			for (int32 i = 0; i < NkModelerState::kAiMesures; ++i)
				if (st.aiMesures[i].id == id) return &st.aiMesures[i];
			return nullptr;
		}
		/// Pose une entree, en recyclant d'abord celles dont l'identifiant a QUITTE
		/// la fenetre du fil.
		/// ⚠️ UNE ENTREE ORPHELINE SE SUPPRIME, elle ne se replie pas sur un
		///    voisin : mieux vaut perdre la mesure d'un bloc qu'on ne voit plus que
		///    l'attribuer a un autre. C'est la meme regle que `BasculerParId`, qui ne
		///    bascule rien quand l'identifiant a disparu.
		inline NkModelerState::AiMesure *NkAiMesurePoser(NkModelerState &st, uint32 id) {
			uint32 tmp = 0;
			for (int32 i = 0; i < NkModelerState::kAiMesures; ++i) {
				NkModelerState::AiMesure &m = st.aiMesures[i];
				if (m.id != 0u && !st.aiFil.TrouverParId(m.id, tmp))
					m = NkModelerState::AiMesure{}; // orpheline : elle s'efface
			}
			for (int32 i = 0; i < NkModelerState::kAiMesures; ++i)
				if (st.aiMesures[i].id == 0u) {
					st.aiMesures[i] = NkModelerState::AiMesure{};
					st.aiMesures[i].id = id;
					return &st.aiMesures[i];
				}
			return nullptr; // table pleine : on ne mesure pas plutot que d'ecraser
		}

		// ── L'HISTORIQUE ─────────────────────────────────────────────────────
		// ⚠️ IL A QUITTE CE FICHIER LE 21/09 : il vit dans le KIT
		//    (`NkAiPanneau::NouvelleConversation` / `Rouvrir`), et il est PAR
		//    ASSISTANT -- l'ancien etait un seul historique pour trois
		//    fournisseurs. Le banc C de `NKAiFilTest` le mesure la-bas.

		// ── LE TOUR, UNE FOIS QUE LA TABLE A REPONDU ────────────────
		// Appelee par la boucle APRES `NkVpPoserAction` : c'est la table qui sait si
		// le verbe existe, et `aiMotifEstRefus` porte deja sa reponse. Les compteurs
		// d'AVANT sont passes par la boucle, seule a voir l'hote -- le panneau ne
		// connait pas le maillage et ne doit pas l'apprendre.
		inline void NkAiTour(NkModelerState &st, const char *demande, int32 v0, int32 e0, int32 f0,
				 int32 frame) {
			if (st.aiMotifEstRefus) {
				// ⚠️ LE MOTIF PASSE MAINTENANT A LA POUSSEE. Le fil refuse un bloc
				//    « refus » sans motif -- « ca n'a pas marche » envoie chercher au
				//    hasard. Avant, il etait recopie APRES coup dans `detail`.
				(void)NkAiPousser(st, NkModelerState::AiType::Refus, st.aiMotif);
				st.aiEnCoursId = 0u; // rien a mesurer : rien n'a ete pose
				return;
			}
			const uint32 id = NkAiPousser(st, NkModelerState::AiType::Operation, demande);
			if (id == 0u)
				return; // le fil a refuse, et il a dit pourquoi
			if (NkModelerState::AiMesure *m = NkAiMesurePoser(st, id)) {
				m->vA = v0;
				m->eA = e0;
				m->fA = f0;
				m->etat = 0;
			}
			st.aiEnCoursId = id;
			st.aiEnCoursFrame = frame;
		}

		// ── L'EFFET, LU DANS LES COMPTEURS ET NULLE PART AILLEURS ──────
		inline void NkAiEffet(NkModelerState &st, int32 v1, int32 e1, int32 f1) {
			editorkit::NkAiBlocDonnees *b = st.aiFil.MutableParId(st.aiEnCoursId);
			NkModelerState::AiMesure *m = NkAiMesureDe(st, st.aiEnCoursId);
			if (!b || !m)
				return; // le bloc a quitte la fenetre : on ne mesure rien plutot que le voisin
			m->vB = v1;
			m->eB = e1;
			m->fB = f1;
			const bool bouge = (v1 != m->vA) || (e1 != m->eA) || (f1 != m->fA);
			m->etat = bouge ? 1u : 2u;
			char out[128];
			if (bouge)
				snprintf(out, sizeof(out), "sommets %d -> %d   aretes %d -> %d   faces %d -> %d",
						 m->vA, v1, m->eA, e1, m->fA, f1);
			else
				// ⚠️ « RIEN N'A CHANGE » N'EST PAS UN ECHEC, et il ne faut pas l'ecrire
				//    comme tel : changer de sous-mode, cadrer la vue ou basculer le rayon X
				//    ne touche aucun compteur. Le dire est plus honnete que d'afficher
				//    « 6 -> 6 » comme un resultat.
				snprintf(out, sizeof(out), "les comptes n'ont pas bouge (%d/%d/%d)", m->vA, m->eA,
						 m->fA);
			b->sortie = NkString(out);
			// L'EFFET COURT, celui qui tient sur la ligne a cote de la demande.
			char eff[64];
			if (bouge)
				snprintf(eff, sizeof(eff), "faces %d -> %d", m->fA, f1);
			else
				snprintf(eff, sizeof(eff), "comptes inchanges");
			b->effet = NkString(eff);
			// -- L EFFET MESURE, LISIBLE PAR UN BANC (NK_AI_TRACE) --
			// Il existait deja, mais seulement DANS le bloc du fil : pour le lire il
			// fallait ouvrir le panneau et regarder. Un verdict qui n existe que la ou
			// personne ne le cherche ne sert a personne.
			//
			// [!] CETTE LIGNE N A JAMAIS TEMOIGNE, ET JE L ECRIS PLUTOT QUE DE LA
			//     LIVRER COMME ACQUISE. Son appelant (main.cpp) ne l atteint que si
			//     `Demo3DHostStats` rend true, et cette fonction n est renseignee QU EN
			//     MODE EDITION. Quatre essais avec `NK_EDIT_MODE=1` ont donne
			//     « AI RESULTAT : acceptee » et un fil a deux blocs sans jamais une
			//     ligne AI EFFET. Ne pas lire son ABSENCE comme « rien n a change » :
			//     c est une sonde muette, pas un zero.
			static const bool sEffetOn = (std::getenv("NK_AI_TRACE") != nullptr);
			if (sEffetOn) {
				std::printf("[nk3d] AI EFFET : %s -> %s\n", b->entree.CStr(), out);
				std::fflush(stdout);
			}
			st.aiEnCoursId = 0u;
		}

		/// L'IDENTIFIANT du DERNIER bloc d'operation du fil, ou 0.
		/// ⚠️ SEUL CELUI-LA PEUT PORTER « Annuler cette action ». Notre pile
		///    d'annulation a UN CRAN : un bouton actif sur une ligne ancienne
		///    annulerait LA DERNIERE en affichant le texte D'UNE AUTRE, et
		///    l'utilisateur verrait le mauvais effet disparaitre.
		inline uint32 NkAiDerniereOperation(const NkModelerState &st) {
			for (uint32 i = st.aiFil.Taille(); i > 0; --i)
				if (st.aiFil.At(i - 1).type == editorkit::NkAiBloc::Outil)
					return st.aiFil.At(i - 1).id;
			return 0u;
		}

		// ══════════════════════════════════════════════════════════════
		//  21/09 — LE PANNEAU EST CELUI DU KIT, ET CE FICHIER NE DESSINE PLUS RIEN
		// ══════════════════════════════════════════════════════════════
		// Rodolf, 21/09 a 04h, capture 040803 a l'appui : le modeleur portait
		// encore les onglets `Local / Claude / Ollama` et les boutons `Envoyer /
		// Fermer`, que le §6 de la spec supprimait la veille ; l'en-tete et le
		// composeur du kit existaient avec ZERO appelant. « Soit tu ne l'as pas
		// implemente, soit tu l'as implemente et ne l'as pas lie. » Les deux.
		//
		// Ce qui PART : les onglets, « Envoyer », « Fermer », la ligne d'etat
		// ecrite ici, le popover d'historique, `NkAiArchiver` & co. -- le kit les
		// porte (`NkAiPanneau`), une fois pour les trois applications.
		// Ce qui RESTE ICI : ce que le modeleur est seul a savoir -- ses trois
		// fournisseurs et leur etat (publie par la boucle), son vocabulaire de
		// verbes (le « / »), le bouton « Annuler cette action » et ce qu'il fait.
		//
		// ⚠️ LA CONVERSATION VIVANTE RESTE `st.aiFil`. Quarante sites l'ecrivent
		//    (boucle, recolte, sondes) ; `Lier` dit au panneau qu'elle vit la, et
		//    changer d'assistant la RANGE dans la case de l'ancien. C'est ce qui
		//    donne une conversation PAR ASSISTANT sans toucher a la boucle.
		// ⚠️ `st.aiOnglet` RESTE L'AUTORITE DE LA BOUCLE (elle choisit le dorsal
		//    par lui). Le panneau le lit en entrant -- `NK_AI_ONGLET` des sondes
		//    l'ecrit -- et le reecrit en sortant : un seul entier, deux lecteurs.

		/// La police a chasse fixe du panneau, chargee et TELEVERSEE par main.cpp
		/// (le code des compartiments IN / OUT). `nullptr` : repli sur la police
		/// d'interface, dit par le peintre.
		inline const nkgui::NkGuiFont *&NkAiPoliceMono() {
			static const nkgui::NkGuiFont *p = nullptr;
			return p;
		}

		/// Les trois fournisseurs du modeleur, DANS L'ORDRE DE `st.aiOnglet` (0 local,
		/// 1 Claude, 2 Ollama). Declares une fois ; leur ETAT (pret, motif) est relu
		/// a chaque image dans ce que la boucle publie -- le panneau ne redecide pas.
		inline void NkAiDeclarerPanneau(NkModelerState &st) {
			editorkit::NkAiPanneau &pan = st.aiPanneau;
			if (pan.fournisseurs.Size() == 0) {
				editorkit::NkAiFournisseurDesc loc;
				loc.cle = NkString("local");
				loc.nom = NkString("Local");
				editorkit::NkAiModeleDesc ml;
				// LE MODELE QUE LE SCRIPT EMPLOIERA VRAIMENT : `ia_verbe.py` lit
				// NK_IA_MODELE, qwen2.5:7b-instruct par defaut. Un gabarit NK_IA_CMD
				// remplace le script entier : on ne pretend alors plus savoir.
				const char *cmd = std::getenv("NK_IA_CMD");
				const char *mod = std::getenv("NK_IA_MODELE");
				if (cmd && *cmd) {
					ml.nom = NkString("NK_IA_CMD");
					ml.detail = NkString("commande posee");
				} else {
					ml.nom = NkString((mod && *mod) ? mod : "qwen2.5:7b-instruct");
					ml.detail = NkString("ia_verbe.py");
				}
				loc.modeles.PushBack(ml);
				pan.fournisseurs.PushBack(loc);
				editorkit::NkAiFournisseurDesc cl;
				cl.cle = NkString("claude");
				cl.nom = NkString("Claude");
				cl.distant = true;
				// Les ALIAS que le CLI accepte (`--model`). Le choix est ecrit dans
				// `st.aiClaudeModele`, que la boucle pose sur le dorsal.
				static const char *const kModeles[3] = {"sonnet", "opus", "haiku"};
				for (int32 i = 0; i < 3; ++i) {
					editorkit::NkAiModeleDesc m;
					m.nom = NkString(kModeles[i]);
					cl.modeles.PushBack(m);
				}
				pan.fournisseurs.PushBack(cl);
				editorkit::NkAiFournisseurDesc ol;
				ol.cle = NkString("ollama");
				ol.nom = NkString("Ollama");
				pan.fournisseurs.PushBack(ol);

				editorkit::NkAiCapacites cap = editorkit::NkAiCapacites::Texte();
				cap.produitOutil = true; // une operation porte une entree et une sortie
				cap.produitRefus = true; // un verbe inconnu est une REPONSE, pas une panne
				pan.capacites = cap;
				// ⚠️ LA FENETRE RESTE A 16, et c'est DECLARE (le 20/09 le defaut du
				//    kit l'avait fait passer a 200 en silence).
				pan.plafond = 16;
				pan.Lier(&st.aiFil);
				pan.indication = NkString("Ce que l'assistant sait faire : une phrase (« subdivise le cube deux "
										  "fois ») ou un verbe du contrat (`subdivide:3`, `bevel:0.2:4`, `undo`). "
										  "Le « / » les liste.");
				pan.invite = NkString("Decrivez l'operation sur la selection…");
				pan.declaration = NkString("Local : NK_IA_CMD (commande) ou NK_IA_MODELE. Claude : le CLI « claude », "
										   "compte NK_CLAUDE_COMPTE.");
				// LE « / » : le VOCABULAIRE DU CONTRAT, lu dans sa table -- une seule
				// autorite, celle que le pont et l'imprimeur du contrat lisent deja.
				int32 nv = 0;
				const NkVerbe *V = NkVerbes(nv);
				for (int32 i = 0; i < nv; ++i) {
					editorkit::NkAiCommandeDesc c;
					c.nom = NkString(V[i].nom);
					c.detail = NkString(V[i].effet ? V[i].effet : "");
					c.insertion = NkString(V[i].nom);
					pan.commandes.PushBack(c);
				}
				// le modele de Claude deja pose (NK_CLAUDE_MODELE) est la valeur courante
				for (int32 i = 0; i < 3; ++i)
					if (st.aiClaudeModeleCourant[0] && NkString(kModeles[i]) == NkString(st.aiClaudeModeleCourant))
						pan.fournisseurs[1].modele = i;
			}
			for (int32 i = 0; i < 3 && i < (int32)pan.fournisseurs.Size(); ++i) {
				pan.fournisseurs[(usize)i].pret = st.aiPretDe[i];
				pan.fournisseurs[(usize)i].motif = NkString(st.aiMotifDe[i]);
			}
			pan.occupe = st.aiEnvoiEnCours;
		}

		/// L'assistant, CONTENU du panneau de droite (Rodolf, 20/09 : « la pastille
		/// de IA doit s'ouvrir sur le panel de droite comme tout le monde »).
		/// `peutAnnuler` vient de l'HOTE : c'est lui qui tient la pile.
		inline void PaintAiDansPanneau(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									   nkgui::NkGuiContext *guiCtx, const NkRect &r, bool peutAnnuler) {
			if (!st.aiOuvert || !guiCtx)
				return;
			NkAiDeclarerPanneau(st);
			editorkit::NkAiPanneau &pan = st.aiPanneau;
			// L'onglet a pu etre ecrit par la boucle ou une sonde : le panneau suit.
			if (st.aiOnglet != pan.Actif()) {
				NkString pq;
				if (!pan.Choisir(st.aiOnglet, pq))
					st.aiOnglet = pan.Actif();
			}
			// ── « ANNULER CETTE ACTION », RENDU (regression du 20/09, reparee) ──
			// ⚠️ SUR LA DERNIERE OPERATION SEULEMENT : notre pile a UN cran pour
			//    l'assistant -- un bouton sur une ligne ancienne annulerait la
			//    derniere en affichant le texte d'une autre.
			// ⚠️ `peutAnnuler` EST LU, PLUS SEULEMENT RECU. C'etait le defaut : le
			//    parametre arrivait, juste, et personne ne le lisait.
			pan.actions = editorkit::NkAiActionsFil{};
			const uint32 derniere = NkAiDerniereOperation(st);
			if (derniere != 0u) {
				pan.actions.blocId = derniere;
				pan.actions.libelle[0] = "Annuler cette action";
				pan.actions.actif[0] = peutAnnuler;
			}
			pan.echelle = S(1.f);
			static NkChrono sHorloge;
			const float64 t = sHorloge.Elapsed().ToSeconds();
			pan.maintenant = t;
			pan.phase = (float32)(t - (float64)(int64)t);
			// L'EMPRISE : le panneau reclame SON rectangle. Le survol ne lui revient
			// que s'il est la surface du dessus -- un menu deroule reste prioritaire.
			const bool libre = hit.Add("ai.panneau", r);
			editorkit::NkGuiComponentPaint pc(*guiCtx, p.Theme());
			pc.PoserPolices(nullptr, NkAiPoliceMono());
			const editorkit::NkAiSorties out = pan.Dessiner(*guiCtx, pc, {r.x, r.y, r.w, r.h}, libre);
			st.aiOnglet = pan.Actif();

			// ── CE QUE LE PANNEAU PUBLIE, pour les sondes (jamais recalcule) ──
			st.aiPanRect[0] = r.x;
			st.aiPanRect[1] = r.y;
			st.aiPanRect[2] = r.w;
			st.aiPanRect[3] = r.h;
			st.aiPlan = pan.planFil;
			st.aiPlanOrigine[0] = pan.filOx;
			st.aiPlanOrigine[1] = pan.filOy;
			// LE BOUTON A SON PROPRE RECTANGLE PUBLIE -- plus celui de l'effet.
			st.aiActionRect[0] = st.aiActionRect[1] = st.aiActionRect[2] = st.aiActionRect[3] = 0.f;
			{
				editorkit::NkAiRectPublie ab;
				if (derniere != 0u && pan.planFil.TrouverIndice(derniere, editorkit::NkAiPiece::Action, 0u, ab)) {
					st.aiActionRect[0] = ab.x + pan.filOx;
					st.aiActionRect[1] = ab.y + pan.filOy;
					st.aiActionRect[2] = ab.w;
					st.aiActionRect[3] = ab.h;
				}
			}
			st.aiComposeurActif = pan.ComposeurActif(*guiCtx);

			// ── LES GESTES ──
			if (out.envoyer) {
				// LA RECOLTE EST ICI, et pas dans `NkAiSoumettre` : c'est le seul
				// endroit ou une phrase TAPEE entre (le bouton « Annuler » et les
				// crochets de mesure soumettent aussi, et ne sont pas Rodolf).
				NkAiRecolter(out.texte.CStr());
				if (NkAiSoumettre(st, out.texte.CStr()))
					pan.ViderSaisie(); // la demande est PARTIE
			}
			if (out.arreter)
				st.aiArreter = true;
			if (out.actionBloc != 0u && out.actionBloc == derniere && out.actionIndice == 0)
				// LA MEME PORTE QUE Ctrl+Z : le verbe `undo`, par le point d'entree unique.
				(void)NkAiSoumettre(st, "undo");
			if (out.modeleChange && pan.Actif() == 1) {
				const editorkit::NkAiFournisseurDesc &f = pan.fournisseurs[1];
				if (f.modele >= 0 && f.modele < (int32)f.modeles.Size())
					NkAiCopie(st.aiClaudeModele, sizeof(st.aiClaudeModele), f.modeles[(usize)f.modele].nom.CStr());
			}
		}

	} // namespace nk3d
} // namespace nkentseu
