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
//  La forme est ecrite dans `echanges/PANNEAU_IA_SPEC.md` : panneau ancre a
//  droite sur toute la hauteur, une barre d'onglets PAR FOURNISSEUR, une ligne
//  de titre, un fil de BLOCS TYPES replies par defaut, un composeur en bas, et
//  sous lui une ligne d'etat.
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
//  DEUX DETTES NOMMEES, AVEC LEUR CONDITION DE RETRAIT :
//    - La specification demande l'entree et la sortie **a chasse fixe**. Ce
//      peintre n'a qu'une police. Le bloc est distingue par son FOND. A
//      corriger le jour ou le shell charge une seconde police.
//    - La specification demande que le panneau soit monte depuis un document
//      `.nkgui`. Le mecanisme de zone hote vient d'arriver par transit ; le
//      panneau doit d'abord exister et se mesurer. Le jour venu, seule
//      l'ORIGINE des rectangles change, pas ce qu'ils montrent.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkAiThreadLayout.h" // le PLAN du fil, commun aux applications
#include "NKEditorKit/NkAiThreadPaint.h"  // sa transcription en commandes
#include "NK3DModeler/Shell/NkModelerComponentPaint.h" // NkModelerPainter vu comme NkComponentPaint

// ⚠️ IL INCLUT CE QU'IL UTILISE. `snprintf` arrivait ici PAR CHANCE, tire par un
//    en-tete voisin ; la recolte ajoute `fopen`/`fputs`/`FILE`, et compter sur
//    la meme chance serait la faute exacte qui a coute 20 erreurs a NKPA -- un
//    en-tete qui compilait chez ses deux consommateurs d'alors, et cassait chez
//    le premier qui ne tirait pas sa dependance.
#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		/// La mesure de texte que le plan demande, servie par la police du peintre.
		/// ⚠️ UNE SEULE POLICE ICI, et le plan demande quand meme la chasse fixe :
		///    c'est le peintre qui se rabat, pas le plan qui ment. Dette nommee,
		///    levee le jour ou la coquille chargera une seconde police.
		inline float32 NkAiMesurerTexte(void *ctx, editorkit::NkAiPolice, const char *t) {
			return ctx ? ((NkModelerPainter *)ctx)->TextW(t) : 0.f;
		}


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
			// LE SUJET EST LA PREMIERE DEMANDE, comme dans la capture.
			if (!st.aiSujet[0])
				NkAiCopie(st.aiSujet, sizeof(st.aiSujet), texte);
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

		// ── L HISTORIQUE : ARCHIVER, ROUVRIR, VIDER ───────────────
		// ⚠️ FENETRE GLISSANTE, COMME LE FIL : on perd la plus ancienne, jamais
		//    la plus recente. Et le sujet voyage AVEC l archive -- sans lui, la
		//    liste afficherait huit lignes identiques.
		inline void NkAiArchiver(NkModelerState &st) {
			if (st.aiFil.Taille() == 0)
				return; // rien a garder : on n archive pas du vide
			if (st.aiArchivesN >= NkModelerState::kAiArchives) {
				for (int32 i = 1; i < NkModelerState::kAiArchives; ++i) {
					st.aiArchives[i - 1] = st.aiArchives[i];
					NkAiCopie(st.aiArchivesSujet[i - 1], 80, st.aiArchivesSujet[i]);
				}
				st.aiArchivesN = NkModelerState::kAiArchives - 1;
			}
			st.aiArchives[st.aiArchivesN] = st.aiFil;
			NkAiCopie(st.aiArchivesSujet[st.aiArchivesN], 80,
					  st.aiSujet[0] ? st.aiSujet : "Sans sujet");
			++st.aiArchivesN;
		}
		/// Rouvre une archive. ⚠️ LA CONVERSATION COURANTE EST ARCHIVEE D ABORD :
		///    rouvrir ne doit jamais faire perdre ce qui etait a l ecran.
		inline void NkAiRouvrir(NkModelerState &st, int32 i) {
			if (i < 0 || i >= st.aiArchivesN)
				return;
			editorkit::NkAiFil garde = st.aiArchives[i];
			char sujet[80];
			NkAiCopie(sujet, sizeof(sujet), st.aiArchivesSujet[i]);
			// On retire l archive rouverte, puis on archive la courante a sa place.
			for (int32 k = i + 1; k < st.aiArchivesN; ++k) {
				st.aiArchives[k - 1] = st.aiArchives[k];
				NkAiCopie(st.aiArchivesSujet[k - 1], 80, st.aiArchivesSujet[k]);
			}
			--st.aiArchivesN;
			NkAiArchiver(st);
			st.aiFil = garde;
			NkAiCopie(st.aiSujet, sizeof(st.aiSujet), sujet);
			st.aiEnCoursId = 0u;
			st.aiDefile = 0.f;
		}
		/// Vide l historique. ⚠️ IL DOIT ETRE ATTEIGNABLE : un historique qu on ne
		///    peut pas vider est une dette de vie privee.
		inline void NkAiViderHistorique(NkModelerState &st) {
			for (int32 i = 0; i < NkModelerState::kAiArchives; ++i) {
				st.aiArchives[i].Vider();
				st.aiArchivesSujet[i][0] = 0;
			}
			st.aiArchivesN = 0;
		}

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

		// ── LE PANNEAU ─────────────────────────────────────────────────────────
		// `r` est le rectangle COMPLET du panneau (ancre a droite, pleine hauteur).
		// `peutAnnuler` vient de l'hote (`Demo3DHostEditCanUndo`) : le panneau ne
		// connait pas le maillage.
		// ══════════════════════════════════════════════════════════════
		//  L ASSISTANT EST LE CONTENU DU PANNEAU DE DROITE, PAS UN PANNEAU A LUI
		// ══════════════════════════════════════════════════════════════
		// Rodolf, 20/09 au soir : « la pastille de IA doit s ouvrir sur le panel de
		// droite comme tout le monde, il ne doit pas avoir son propre panel. »
		//
		// La fonction ne PEINT PLUS DE PANNEAU : elle remplit le corps qu on lui
		// donne -- celui du panneau de droite -- comme une section de proprietes.
		// Partent donc le fond, le filet de gauche, et l emprise `ai.box`.
		//
		// ⚠️ CE N EST PAS LE « PANNEAU DANS UN PANNEAU HOTE » QUE LA SPECIFICATION
		//    INTERDIT, et la distinction est celle qui compte : ce defaut-la -- les
		//    clics une image sur deux, deux menus de NKUIDesign -- vient d un
		//    panneau FLOTTANT dessine dans un hote, avec ses propres couches et sa
		//    propre emprise. Ici il n y a plus de panneau du tout : c est le CONTENU
		//    de l hote qui change, comme quand on passe de « Materiau » a
		//    « Lumiere ». Une seule couche, une seule emprise.
		//
		// ⚠️ `aiPanRect` RESTE PUBLIE : deux sondes le lisent, et il dit desormais
		//    le corps occupe DANS l hote. Le temoin garde son critere -- la pastille
		//    et le contenu ne se recouvrent pas -- et il reste vrai.
		inline void PaintAiDansPanneau(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								   nkgui::NkGuiContext *guiCtx, const NkRect &r, bool peutAnnuler) {
			if (!st.aiOuvert)
				return;
			// ── VU, PARCE QU'IL EST PEINT ──────────────────────────────
			// La marque se consomme ICI et nulle part ailleurs : au moment ou les blocs
			// passent sous les yeux. La poser au CLIC de la pastille aurait marque
			// « vu » un panneau que ce meme clic venait peut-etre de FERMER.
			st.aiFil.MarquerVus();
			const float32 kRowH = S(22.f);
			const float32 pad = S(8.f);
			// L'EMPRISE, DECLAREE D'ABORD. Sans elle, un clic dans le vide du
			// panneau traverserait jusqu'au viseur et deselectionnerait -- le
			// defaut « les clics traversent » deja paye sur les surcouches de la vue.
			// ⚠️ PLUS D EMPRISE `ai.box`. On ne reclame pas le corps d un panneau
			//    dont on EST le contenu : l hote a deja reclame. La garder aurait pose
			//    une SECONDE reclamation sur la meme zone -- et c est ainsi qu on
			//    fabrique un survol qui gagne une image sur deux.
			// Le panneau PUBLIE le rectangle qu il vient de reclamer. Le temoin le
			// compare a celui de la pastille : deux mesures, deux peintres, aucune
			// formule recopiee dans la sonde.
			st.aiPanRect[0] = r.x; st.aiPanRect[1] = r.y;
			st.aiPanRect[2] = r.w; st.aiPanRect[3] = r.h;
			// Ni fond ni filet : le panneau hote les peint. Un second fond par-dessus
			// le sien serait invisible aujourd hui et faux le jour ou le theme change.

			float32 yy = r.y;

			// ── 1. LA BARRE D'ONGLETS DE FOURNISSEUR ───────────────────────────
			{
				const float32 tw = (r.w - pad * 2.f) / (float32)kAiFournisseurs;
				for (int32 i = 0; i < kAiFournisseurs; ++i) {
					const NkRect tr{r.x + pad + tw * (float32)i, yy + S(4.f), tw, kRowH};
					char cle[24];
					snprintf(cle, sizeof(cle), "ai.tab%d", (int)i);
					const bool over = hit.Add(cle, tr);
					const bool actif = (st.aiOnglet == i);
					p.Fill(tr,
						   actif ? NkRole::PanelHeader : (over ? NkRole::InputBg : NkRole::PanelBg),
						   3.f);
					p.TextV(tr.x + (tw - p.TextW(NkAiFournisseur(i))) * 0.5f, tr.y, kRowH,
							NkAiFournisseur(i), actif ? NkRole::Text : NkRole::TextMuted);
					if (actif)
						p.Fill({tr.x, tr.y + kRowH - S(2.f), tw, S(2.f)}, NkRole::AccentUi, 0.f);
					if (hit.Clicked(cle))
						st.aiOnglet = i;
				}
				yy += kRowH + S(8.f);
			}

			// ── 2. LA LIGNE DE TITRE : sujet tronque, et deux boutons discrets ──
			{
				const float32 bw = S(22.f);
				const NkRect nouv{r.x + r.w - pad - bw, yy, bw, kRowH};
				const NkRect hist{nouv.x - bw - S(4.f), yy, bw, kRowH};
				p.TextClipped(r.x + pad, yy + S(4.f), hist.x - r.x - pad * 2.f,
							  st.aiSujet[0] ? st.aiSujet : "Nouvelle conversation", NkRole::Text);
				const bool ovH = hit.Add("ai.hist", hist);
				// ⚠️ LE GESTE QUI MANQUAIT. Cette cle etait enregistree et son survol
				//    LU (il colore l icone) sans que `Clicked` existe nulle part : une
				//    icone qui s eclaire promet un geste. Elle le tient desormais.
				if (hit.Clicked("ai.hist"))
					st.aiHistOuvert = !st.aiHistOuvert;
				const bool ovN = hit.Add("ai.nouv", nouv);
				p.IconV(hist.x + S(4.f), yy, kRowH, NkIcon::Journal,
						ovH ? NkRole::Text : NkRole::TextMuted, 12.f);
				p.IconV(nouv.x + S(4.f), yy, kRowH, NkIcon::PlusCircle,
						ovN ? NkRole::Text : NkRole::TextMuted, 12.f);
				if (hit.Clicked("ai.nouv")) {
					// NOUVELLE CONVERSATION : le fil se vide, le sujet aussi.
					// ⚠️ Rien d'autre. Elle ne touche pas au maillage : une
					//    conversation qu'on ferme ne defait pas ce qu'elle a fait.
					// ⚠️ ON ARCHIVE AVANT DE VIDER. Avant, « nouvelle conversation »
					//    JETAIT le fil : le travail disparaissait sans recours.
					NkAiArchiver(st);
					st.aiFil.Vider();
					st.aiEnCoursId = 0u;
					st.aiSujet[0] = 0;
					st.aiDefile = 0.f;
				}
				yy += kRowH + S(4.f);
				// ── LE POPOVER DE L HISTORIQUE ────────────────────────
				// ⚠️ IL DIT QUE RIEN NE SURVIT A LA FERMETURE, et ce n est pas une note
				//    de bas de page : un historique qui s evapore EN SILENCE fait perdre
				//    du travail qu on croyait garde. La persistance demande un format ;
				//    tant qu il n existe pas, la phrase remplace la promesse.
				if (st.aiHistOuvert) {
					const float32 lh = kRowH;
					const int32 n = st.aiArchivesN;
					const float32 hpop = lh * (float32)(n + 2) + S(8.f);
					const NkRect pop{r.x + pad, yy, r.w - pad * 2.f, hpop};
					(void)hit.Add("ai.pop", pop); // l emprise : les clics ne traversent pas
					p.Outline(pop, NkRole::Border, NkRole::PanelHeader, S(4.f));
					float32 py = pop.y + S(4.f);
					for (int32 a = n - 1; a >= 0; --a) {
						char cle[24];
						snprintf(cle, sizeof(cle), "ai.arch%d", (int)a);
						const NkRect lr{pop.x + S(2.f), py, pop.w - S(4.f), lh};
						if (hit.Add(cle, lr))
							p.Fill(lr, NkRole::InputBg, S(3.f));
						char lib[120];
						snprintf(lib, sizeof(lib), "%s   ·   %u bloc(s)", st.aiArchivesSujet[a],
								 (unsigned)st.aiArchives[a].Taille());
						p.TextClipped(lr.x + S(6.f), py + S(4.f), lr.w - S(12.f), lib, NkRole::Text);
						if (hit.Clicked(cle)) {
							NkAiRouvrir(st, a);
							st.aiHistOuvert = false;
						}
						py += lh;
					}
					if (n == 0) {
						p.TextClipped(pop.x + S(6.f), py + S(4.f), pop.w - S(12.f),
								 "Aucune conversation archivee.", NkRole::TextMuted);
						py += lh;
					}
					// ⚠️ VIDER DOIT ETRE ATTEIGNABLE. Un historique qu on ne peut pas
					//    vider est une dette de vie privee.
					const NkRect vr{pop.x + S(2.f), py, pop.w - S(4.f), lh};
					if (hit.Add("ai.histvide", vr))
						p.Fill(vr, NkRole::InputBg, S(3.f));
					p.TextClipped(vr.x + S(6.f), py + S(4.f), vr.w - S(12.f),
							 n > 0 ? "Vider l historique" : "Vider l historique (rien a vider)",
							 n > 0 ? NkRole::AxisX : NkRole::TextMuted);
					if (n > 0 && hit.Clicked("ai.histvide"))
						NkAiViderHistorique(st);
					py += lh;
					p.TextClipped(pop.x + S(6.f), py + S(2.f), pop.w - S(12.f),
							 "Les conversations ne survivent pas a la fermeture.", NkRole::TextMuted);
					yy += hpop + S(4.f);
				}
			}
			p.Fill({r.x + pad, yy, r.w - pad * 2.f, S(1.f)}, NkRole::Border, 0.f);
			yy += S(6.f);

			// ── 3. LE COMPOSEUR ET SA LIGNE D'ETAT, RESERVES EN BAS ────────────
			// Reserves AVANT de peindre le fil : c'est ce qui donne au fil sa
			// hauteur exacte. L'inverse -- peindre le fil puis « ce qui reste » --
			// laisse le composeur sortir du panneau des que le fil est long.
			const float32 hComposeur = kRowH * 2.f + S(6.f);
			const float32 hEtat = kRowH;
			const float32 yComposeur = r.y + r.h - pad - hEtat - S(6.f) - hComposeur;
			const NkRect filR{r.x + pad, yy, r.w - pad * 2.f, yComposeur - yy - S(6.f)};

			// ── 4. LE FIL DE BLOCS TYPES ── PEINT PAR LE KIT ───────────
			// ⚠️ CE PANNEAU NE DESSINE PLUS SON FIL, ET C'EST TOUT LE LOT.
			//    Rodolf, 20/09 : « que ce soit nk code ou nk3dmodeler ou nkuidesign
			//    [...] je veux que le panneau soit exactement comme ceci ». Trois
			//    panneaux conformes aujourd'hui divergent en un mois ; un composant
			//    partage ne le peut pas.
			//
			//    La geometrie vient de `NkAiFilMesurer` (un PLAN de rectangles
			//    nommes), la peinture de `NkAiFilPeindre`, qui ne calcule aucune
			//    position. L'adaptateur `NkModelerComponentPaint` existait depuis le
			//    18/08 : il n'y avait rien a ecrire pour brancher l'un sur l'autre.
			//
			// ⚠️ LA CHASSE FIXE MANQUE TOUJOURS -- ce peintre n'a qu'une police, et
			//    le plan la demande quand meme. C'est le PEINTRE qui se rabat, pas le
			//    plan qui ment : le jour ou la coquille charge une seconde police,
			//    rien n'est a changer ici. Les blocs `IN`/`OUT` restent distingues
			//    par leur FOND (`code_bg` / `code_out_bg`), comme aujourd'hui.
			p.Clip(filR);
			if (st.aiFil.Taille() == 0) {
				float32 fy = filR.y;
				p.TextV(filR.x, fy, kRowH, "Aucun echange pour l'instant.", NkRole::TextMuted);
				fy += kRowH;
				// ⚠️ ON DIT CE QU'ON SAIT FAIRE. Un champ libre sans exemple se repond
				//    par des phrases que rien ne comprend, et l'utilisateur conclut que
				//    l'outil ne marche pas -- alors qu'il n'a jamais su ce qu'on
				//    attendait de lui.
				p.TextV(filR.x, fy, kRowH, "Ex. : subdivide:3  ·  bevel:0.2:4  ·  undo",
						 NkRole::TextMuted);
			} else {
				editorkit::NkAiMetriques metr;
				// ⚠️ L ECHELLE DE L HOTE, SANS QUOI LE PANNEAU IGNORE `gUiScale`.
				//    Toute la mise en page du modeleur passe par `S(px)` ; le kit est en
				//    pixels bruts. Sans cette ligne le fil resterait a 100 % pendant que
				//    le reste du panneau grandit -- invisible ici (`gUiScale` = 1), et
				//    seulement chez quelqu un qui travaille a 125 %.
				metr.Echelle(S(1.f));
				editorkit::NkAiFilMesurer(st.aiFil, filR.w, metr, NkAiMesurerTexte, &p, st.aiPlan);
				NkModelerComponentPaint pc(p);
				editorkit::NkAiFilPeindre(pc, st.aiFil, st.aiPlan, filR.x, filR.y - st.aiDefile);
				// ⚠️ LE CLIC PASSE PAR L'IDENTIFIANT, PLUS PAR UN INDICE. `ai.b%d`
				//    avec la position etait faux des qu'un bloc entrait dans un fil
				//    plein : mesure du 20/09, l'indice 10 designait « etape 11 ».
				for (uint32 i = 0; i < st.aiFil.Taille(); ++i) {
					const uint32 id = st.aiFil.At(i).id;
					editorkit::NkAiRectPublie rc;
					if (!st.aiPlan.Trouver(id, editorkit::NkAiPiece::Texte, rc) &&
						 !st.aiPlan.Trouver(id, editorkit::NkAiPiece::Titre, rc))
						continue;
					char cle[24];
					snprintf(cle, sizeof(cle), "ai.b%u", (unsigned)id);
					(void)hit.Add(cle, {filR.x, rc.y + filR.y - st.aiDefile, filR.w, metr.ligne});
					if (hit.Clicked(cle))
						st.aiFil.BasculerParId(id); // ⚠️ DEPLIER N'EXECUTE RIEN
				}
			}
			p.Unclip();

			// ── 5. LE COMPOSEUR ────────────────────────────────────────────────
			// ⚠️ CE N'EST PLUS « LOCAL OU RIEN ». Jusqu'au 20/09 le composeur
			//    n'etait allume que sur l'onglet 0, parce qu'aucun autre n'etait
			//    cable. Il l'est maintenant des que la BOUCLE dit que l'onglet a
			//    un dorsal -- `st.aiOngletPret`, ecrit par `NkIaCanal::DorsalDe`.
			//    Le panneau ne redecide pas : deux avis sur la meme question
			//    finissent toujours par diverger, et l'utilisateur croit celui
			//    qu'il voit.
			const bool ongletActif = st.aiOngletPret;
			{
				const float32 bw = S(74.f);
				const NkRect champ{r.x + pad, yComposeur, r.w - pad * 2.f - bw - S(6.f),
								   hComposeur};
				p.Outline(champ, NkRole::Border, NkRole::InputBg, 3.f);
				if (guiCtx && ongletActif) {
					editorkit::NkOverlayTextField(*guiCtx, guiCtx->dl, p.FontPtr(),
												  {champ.x, champ.y, champ.w, kRowH}, st.aiSaisie,
												  (int32)sizeof(st.aiSaisie) - 1, true);
					// ── CE QUI PART, DIT SOUS LE CHAMP, AVANT D'ENVOYER ───────
					// ⚠️ L'AVERTISSEMENT VIT SOUS LE CURSEUR, PAS DANS UNE LIGNE
					//    D'ETAT EN BAS DE PANNEAU. Rodolf doit pouvoir choisir en
					//    connaissance de cause A CHAQUE USAGE, et le regard est
					//    sur le champ qu'il remplit -- pas sur le bord inferieur.
					if (st.aiOngletDistant)
						p.TextClipped(champ.x + S(4.f), champ.y + S(4.f) + kRowH,
									  champ.w - S(8.f),
									  "⚠ SERVICE DISTANT : cette demande et le contrat des 26 "
									  "verbes QUITTENT cette machine.",
									  NkRole::AxisX);
				} else if (!ongletActif) {
					// LE ZERO, ET IL DIT LEQUEL. Le motif vient du canal, avec le
					// GESTE QUI REPARE en tete : « Installez... », « Connectez-vous... ».
					// ⚠️ Un refus muet a cote de deux onglets bavards se lit comme
					//    un silence -- et on cherche le defaut dans sa demande.
					p.TextWrap(champ.x + S(4.f), champ.y + S(4.f), champ.w - S(8.f),
							   st.aiOngletMotif[0] ? st.aiOngletMotif
												   : "Aucun dorsal pour cet onglet.",
							   NkRole::TextMuted);
				} else {
					// Repli nomme (pas de contexte) : on AFFICHE, on n'edite pas. Un
					// champ muet qui a l'air editable est pire qu'un champ eteint.
					p.TextV(champ.x + S(4.f), champ.y, kRowH, st.aiSaisie, NkRole::TextMuted);
				}
				const NkRect bt{r.x + r.w - pad - bw, yComposeur, bw, kRowH};
				const bool over = hit.Add("ai.envoyer", bt);
				const bool actif = ongletActif && !NkAiVide(st.aiSaisie);
				// Le bouton dit lui-meme s'il fera quelque chose : eteint quand le
				// champ est vide. C'est le ZERO, rendu visible avant d'etre clique.
				p.Fill(bt,
					   actif ? (over ? NkRole::AccentUi : NkRole::PanelHeader) : NkRole::InputBg,
					   3.f);
				p.TextV(bt.x + (bw - p.TextW("Envoyer")) * 0.5f, bt.y, kRowH, "Envoyer",
						actif ? NkRole::Text : NkRole::TextMuted);
				if (actif && hit.Clicked("ai.envoyer")) {
					// ── LA RECOLTE EST **ICI**, ET PAS DANS `NkAiSoumettre` ───────
					// ⚠️ C'est le seul endroit ou une phrase TAPEE entre. Poser la
					//    recolte dans `NkAiSoumettre` aurait paru plus sur -- « tout
					//    passe par la » -- et aurait pollue le corpus : ses autres
					//    appelants sont le bouton « Annuler cette action », qui
					//    soumet le verbe `undo`, et le crochet de mesure
					//    `NK_AI_DEMANDE`, qui soumet MES phrases de banc. Le corpus
					//    aurait alors contenu ce que l'application se dit a
					//    elle-meme, melange a ce que Rodolf demande -- et c'est
					//    justement ce melange qu'on cherche a eviter.
					NkAiRecolter(st.aiSaisie);
					if (NkAiSoumettre(st, st.aiSaisie))
						st.aiSaisie[0] = 0; // le champ se vide : la demande est PARTIE
				}
				// FERMER : le panneau est refermable, comme celui de la capture.
				const NkRect fb{bt.x, bt.y + kRowH + S(6.f), bw, kRowH};
				const bool ovF = hit.Add("ai.fermer", fb);
				p.Fill(fb, ovF ? NkRole::InputBg : NkRole::PanelBg, 3.f);
				p.TextV(fb.x + (bw - p.TextW("Fermer")) * 0.5f, fb.y, kRowH, "Fermer",
						NkRole::TextMuted);
				if (hit.Clicked("ai.fermer"))
					// ON RECULE UN GESTE, ON NE LE RETIRE PAS : ce bouton reste, mais
					// il n'est plus la SEULE sortie -- la pastille du bord droit en
					// est une, visible que le panneau soit ouvert ou ferme. Les deux
					// ecrivent le MEME etat : aucune des deux portes ne peut produire
					// un resultat que l'autre ne produirait pas.
					st.aiOuvert = false;
			}

			// ── 6. LA LIGNE D'ETAT : LE DORSAL ET SON COUT ─────────────────────
			// ⚠️ « NON CHARGE » EST LA PARTIE IMPORTANTE. Les 4 444 Mo sont une
			//    mesure d'en-tete du 14/09, pas une mesure de l'instant :
			//    l'afficher seul ferait croire que le modele occupe la carte
			//    maintenant. Et la carte est DISPUTEE -- l'entrainement d'Ilyana y
			//    tourne. Rodolf doit pouvoir comprendre un ralentissement sans le
			//    deviner.
			//
			// ⚠️ CETTE LIGNE N'EST PLUS COMPOSEE ICI, ET C'EST LE CORRECTIF DU
			//    20/09. Le panneau ecrivait le texte PUIS le recopiait dans
			//    `st.aiEtat` « ce qui est AFFICHE, pas une copie » -- mais il le
			//    composait a partir du SEUL `aiOnglet`, sans rien savoir du
			//    dorsal. Avec trois onglets dont deux cables, il aurait fallu lui
			//    apprendre l'etat des dorsaux : c'est-a-dire une SECONDE autorite
			//    sur la meme question. `NkIaCanal::LigneEtat` l'ecrit, la boucle
			//    la depose, le panneau la PEINT. Un seul auteur.
			{
				const NkRect er{r.x + pad, r.y + r.h - pad - hEtat, r.w - pad * 2.f, hEtat};
				// LE DISTANT SE VOIT : fond d'alerte, pas le fond d'en-tete ordinaire.
				// Un avertissement de la meme couleur que tout le reste n'avertit pas.
				p.Fill(er, st.aiOngletDistant ? NkRole::InputBg : NkRole::PanelHeader, 3.f);
				p.TextClipped(er.x + S(6.f), er.y + S(3.f), er.w - S(12.f),
							  st.aiEtat[0] ? st.aiEtat : "(etat non publie par la boucle)",
							  st.aiOngletDistant ? NkRole::AxisX : NkRole::TextMuted);
			}
		}

	} // namespace nk3d
} // namespace nkentseu
