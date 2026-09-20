#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerIA.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LE PANNEAU APPELLE VRAIMENT — la phrase part, le verbe revient
// =============================================================================
//  Jusqu'ici, le texte tape dans le panneau etait soumis TEL QUEL au pont des
//  verbes : « subdivise deux fois » tombait donc dans le refus nomme, parce que
//  ce n'est pas un verbe. Le panneau avait sa forme, son effet mesure et son
//  annulation, mais il n'avait pas de MODELE. Ce fichier pose le chainon
//  manquant, et rien d'autre.
//
//  ⚠️ CE FICHIER NE CONNAIT AUCUN MODELE, ET C'EST UNE CONSIGNE, PAS UN GOUT.
//     Il parle a `NKConverse` -- une requete, une reponse ou un refus nomme --
//     et le choix du dorsal est un REGLAGE. Ollama est un echafaudage qui
//     debloque cette nuit, pas une destination : `Kernel/AI/NKInfer` lit les
//     MEMES poids GGUF (NkOllamaLocate traduit deja « qwen2.5:7b-instruct » en
//     chemin de blob), et NKAI a distille Qwen. Le jour ou notre moteur sert la
//     reponse, on change le GABARIT et pas une ligne d'ici.
//     ⚠️ Si du vocabulaire propre a un fournisseur remonte jusqu'au panneau,
//        c'est une FUITE : elle se signale, elle ne s'absorbe pas.
//
//  ⚠️ ET ON N'ECRIT PAS DE DORSAL. `NKConverse` porte deja l'interface et un
//     dorsal generique de PROCESSUS (« deux trous, {invite} et {sortie} »).
//     Un autre chantier ecrit en ce moment le dorsal HTTP dans ce module
//     partage : c'est le sien qu'il faudra consommer. Deux dorsaux seraient la
//     dette des trois exemplaires, rouverte le lendemain de son remboursement.
//
//  POURQUOI L'ASYNCHRONE. Une reponse mesuree a 0,4 s sur carte peut valoir des
//  dizaines de secondes sur processeur, ou ne jamais venir. Un appel synchrone
//  gelerait la fenetre, et Rodolf croirait l'application plantee. `NKConverse`
//  porte deja `NkEnvoiAsync` : on le CONSOMME.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerContrat.h"
#include "NK3DModeler/Shell/NkModelerInput.h" // NkModelerState, NkVpAction
#include "NKConverse/NkConverseChatAsync.h"
#include "NKConverse/NkConverseClaude.h" // le dorsal DISTANT, choisi par l'onglet

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		/// Le verbe -> l'indice de commande. Pose par `main.cpp`, qui porte la
		/// table : la lui demander evite de la recopier ici, et une recopie
		/// pourrait mentir sans que rien ne le signale.
		inline int32 (*NkIaCmdDuVerbe)(NkVpAction) = nullptr;

		// ── LE CONTRAT, DONNE AU MODELE ────────────────────────────────────────
		// ⚠️ UN MODELE QUI CONNAIT LA GRAMMAIRE N'A PLUS A L'INVENTER. C'est la
		//    raison d'etre de la table de verbes devenue DONNEE : elle est lue
		//    ici comme elle est lue par l'imprimeur du contrat et par le pont.
		//    UNE SEULE AUTORITE, TROIS LECTEURS -- un verbe ajoute est reconnu,
		//    documente ET enseigne au modele du meme geste.
		// ⚠️ Les bornes ne sont pas recopiees : elles viennent de
		//    `Demo3DHostOpParamInfo`, la table que le panneau de proprietes
		//    affiche et que le clamp applique.
		inline void NkIaEcrireContratDansInvite(char *dst, uint32 taille, bool avecContrat) {
			if (!dst || taille == 0)
				return;
			dst[0] = 0;
			uint32 n = 0;
			auto ajout = [&](const char *s) {
				if (!s)
					return;
				while (*s && n + 1u < taille)
					dst[n++] = *s++;
				dst[n] = 0;
			};
			ajout("Tu traduis une demande en UNE commande, et rien d'autre.\n");
			ajout("Reponds par UNE SEULE LIGNE, sans phrase, sans explication, sans ponctuation\n");
			ajout("finale, au format  verbe  ou  verbe:parametre1:parametre2\n");
			if (!avecContrat) {
				// ⚠️ LE NEGATIF DE LA MESURE « AVEC CONTRAT CONTRE SANS ». Sans la
				//    table, le modele doit DEVINER le vocabulaire. C'est ce que
				//    mesure la sonde : ce que NOTRE outillage apporte, et non ce
				//    que le modele sait deja.
				// ⚠️ LA MUTATION NE DOIT CHANGER QU'UNE CHOSE : la presence de la
				//    TABLE. La regle de syntaxe reste dite des deux cotes, sinon on
				//    comparerait « avec table et avec syntaxe » a « sans table et sans
				//    syntaxe » -- deux variables pour une conclusion.
				ajout("Les parametres sont POSITIONNELS : leurs VALEURS, separees par des\n");
				ajout("deux-points. Exemple : bevel:0.2:4\n");
				ajout("Le vocabulaire n'est pas donne : devine le verbe anglais usuel.\n");
				return;
			}
			// ⚠️ CES QUATRE LIGNES VIENNENT D'UNE MESURE, PAS D'UNE INTUITION. Sans
			//    elles, le modele recopiait les LIBELLES du tableau comme s'ils
			//    etaient de la syntaxe et rendait
			//        bevel:largeur (0 = auto)=0.5:segments=4
			//    au lieu de `bevel:0.2:4`. Ce n'etait pas une faute du modele : notre
			//    contrat melangeait un libelle destine a un humain et une syntaxe
			//    destinee a une machine, sans jamais dire lequel etait lequel.
			ajout("\nLes parametres sont POSITIONNELS : on ecrit leurs VALEURS, dans l'ordre du\n");
			ajout("tableau, separees par des deux-points. On n'ecrit JAMAIS le nom d'un\n");
			ajout("parametre. Exemple : pour biseauter a 0.2 avec 4 segments -> bevel:0.2:4\n");
			ajout("Un verbe sans parametre s'ecrit seul : subdivide\n");
			// ⚠️ CES DEUX REGLES VIENNENT AUSSI D'UNE MESURE, ET JE DIS D'OU : des DIX
			//    demandes du banc. Le contrat portait T1 et T2 a 10/10 mais FAISAIT
			//    BAISSER T3, parce qu'il rendait le modele trop zele : il remplissait
			//    des parametres que personne n'avait demandes (« extrude:0:0 » alors
			//    qu'extrude n'en a qu'UN -- le pont l'aurait signale comme parametre
			//    en trop) et il trouvait TOUJOURS un verbe, meme pour « rends ce
			//    modele plus beau ».
			//    ⚠️ AVOIR TIRE CES REGLES DES CAS DU BANC REND LE TAUX SUIVANT
			//       OPTIMISTE. Ce sont des regles GENERALES et non des reponses
			//       apprises par cœur, mais le banc n'est plus tout a fait aveugle,
			//       et un taux mesure sur ce qui a servi a regler se lit avec cette
			//       reserve. De nouvelles demandes le diront mieux.
			ajout("N'AJOUTE JAMAIS un parametre que la demande ne donne pas : si elle ne\n");
			ajout("precise rien, ecris le verbe SEUL. Ne donne jamais plus de valeurs que le\n");
			ajout("tableau n'en annonce pour ce verbe.\n");
			ajout("Si la demande ne correspond a AUCUN verbe de la liste -- parce qu'elle\n");
			ajout("designe par les mots, demande de creer un objet, ou reste vague --\n");
			ajout("reponds exactement : aucune. Ne choisis pas un verbe par defaut.\n");
			ajout("\nVerbes autorises, avec leurs parametres et leurs bornes :\n");
			int32 nv = 0;
			const NkVerbe *V = NkVerbes(nv);
			const int32 np = demo::Demo3DHostOpParamCount();
			for (int32 i = 0; i < nv; ++i) {
				char ligne[256];
				int32 pos = snprintf(ligne, sizeof(ligne), "- %s", V[i].nom);
				const int32 cmd = NkIaCmdDuVerbe ? NkIaCmdDuVerbe(V[i].act) : -1;
				int32 ecrits = 0;
				for (int32 p = 0; p < np && pos > 0 && pos < (int32)sizeof(ligne); ++p) {
					int32 pc = -1, type = 0;
					const char *lib = "";
					float32 vmin = 0.f, vmax = 0.f;
					if (!demo::Demo3DHostOpParamInfo(p, &pc, &lib, &type, &vmin, &vmax))
						continue;
					if (cmd < 0 || pc != cmd)
						continue;
					pos += snprintf(ligne + pos, sizeof(ligne) - (size_t)pos, "%s%s (%s, %g a %g)",
									ecrits ? ", " : " : ", lib,
									type == 0 ? "0 ou 1" : (type == 1 ? "entier" : "reel"),
									(double)vmin, (double)vmax);
					++ecrits;
				}
				if (pos > 0 && pos < (int32)sizeof(ligne))
					snprintf(ligne + pos, sizeof(ligne) - (size_t)pos, " -- %s\n", V[i].effet);
				ajout(ligne);
			}
			ajout("\nUne valeur hors bornes est ramenee aux bornes ; un parametre en trop est\n");
			ajout("refuse.\n");
		}

		// ── LA REPONSE -> UN VERBE ─────────────────────────────────────────────
		// ⚠️ ON NE FAIT PAS CONFIANCE A LA FORME DE LA REPONSE. Un modele ajoute
		//    des guillemets, des puces, des blocs de code, une phrase de
		//    politesse. On prend la PREMIERE ligne qui ressemble a un verbe, et
		//    si rien ne ressemble, on REFUSE EN LE DISANT plutot que de soumettre
		//    une phrase au pont -- qui la refuserait avec un motif exact mais
		//    incomprehensible pour Rodolf (« je ne connais pas "voici la
		//    commande :" »).
		inline bool NkIaExtraireVerbe(const char *reponse, char *dst, uint32 taille) {
			if (!dst || taille == 0)
				return false;
			dst[0] = 0;
			if (!reponse)
				return false;
			const char *c = reponse;
			while (*c) {
				// debut de ligne : on saute les ornements usuels
				while (*c == ' ' || *c == '\t' || *c == '-' || *c == '*' || *c == '`' || *c == '"' ||
					   *c == '\'' || *c == '>')
					++c;
				const char *deb = c;
				while (*c && *c != '\n' && *c != '\r')
					++c;
				const char *fin = c;
				while (fin > deb && (fin[-1] == ' ' || fin[-1] == '\t' || fin[-1] == '.' ||
									 fin[-1] == '`' || fin[-1] == '"' || fin[-1] == '\'' ||
									 fin[-1] == ';'))
					--fin;
				if (fin > deb) {
					uint32 n = 0;
					for (const char *p = deb; p < fin && n + 1u < taille; ++p)
						dst[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
					dst[n] = 0;
					// Un verbe n'a ni espace ni accent : « subdivide:2 » oui,
					// « voici la commande » non.
					bool plausible = n > 0;
					for (uint32 k = 0; k < n && plausible; ++k) {
						const char x = dst[k];
						const bool ok = (x >= 'a' && x <= 'z') || (x >= '0' && x <= '9') || x == ':' ||
										x == '.' || x == '-' || x == '+' || x == '_';
						if (!ok)
							plausible = false;
					}
					if (plausible)
						return true;
					dst[0] = 0;
				}
				while (*c == '\n' || *c == '\r')
					++c;
			}
			return false;
		}

		// =====================================================================
		//  LA BOUCLE — « fais X jusqu'a ce que Y »
		// =====================================================================
		//  ⚠️ CE QUI EST MESURE, ET CE QUI NE L'EST PAS. L'addendum ci-dessous est
		//     le texte EXACT du banc P1/P2 du 20/09 :
		//       claude/sonnet : P1 12/12 sur les trois axes, P2 8/8, 0 predicat invente
		//       qwen2.5:7b    : P1 7/12 (triplet), P2 5/8, 3 predicats INVENTES
		//     Seuil ecrit d'avance : P1 >= 10 ET P2 >= 6. Claude passe, qwen non.
		//     ⚠️ CE QUI N'EST PAS MESURE : la sortie A DEUX LIGNES (le verbe PUIS
		//        le predicat) dans un MEME appel. P1 ne demandait que le predicat,
		//        le verbe venant de M1 (10/10) dans un appel separe. Les deux
		//        moities sont mesurees, **leur reunion ne l'est pas.** Elle se
		//        mesure par 12 appels, et tant que ce n'est pas fait, ce chiffre
		//        n'existe pas -- ne pas lire le 12/12 de P1 comme s'il couvrait
		//        cette forme.
		//
		//  ⚠️ LE MODELE EST APPELE **UNE FOIS**, PAS UNE FOIS PAR TOUR. Il traduit
		//     la demande en (verbe, predicat) et sort de la boucle. Ensuite
		//     l'application applique, LIT ses compteurs, evalue, recommence. Les
		//     tours suivants ne quittent pas la machine et ne coutent rien.
		//     C'est la forme que la mesure U7 donnait deja a 10/10 : *la condition
		//     d'arret appartient a la boucle, pas au modele.*

		// =====================================================================
		//  LES LOIS DE CROISSANCE : CE QUI EST MESURE, ET CE QUI NE L'EST PAS
		// =====================================================================
		//  ⚠️ CETTE LISTE EXISTE PARCE QUE `NkIaPasSur` NE PEUT PREDIRE QUE CE
		//     QU'ON A MESURE. Elle est ecrite pour que la personne suivante
		//     trouve le travail fait, et non pour qu'elle croie les chiffres :
		//     il n'y en a qu'UN ici.
		//
		//  LES SEPT VERBES BOUCLABLES SE PARTAGENT EN DEUX FAMILLES, et la
		//  distinction n'est pas cosmetique -- elle decide QUEL plafond garde :
		//
		//  (A) CEUX QUI FONT CROITRE  -> le plafond de FACES est le risque.
		//      1. `subdivide[:k]`  **MESURE** : faces x 4^k. Mesure du 17/09,
		//         6 -> 24 pour k=1 et 6 -> 384 pour k=3. C'est la SEULE loi
		//         etablie, et la seule que la garde predit.
		//      2. `loopcut:n[:glissement]`  **NON MESURE**. Depend du nombre de
		//         boucles n (1 a 5) ET de la topologie de la boucle d'aretes
		//         traversee : un anneau de m quads coupe n fois ajoute ~n*m
		//         faces. Ce n'est donc PAS un facteur, c'est un ADDITIF qui
		//         depend de la geometrie -- une loi de la meme forme que
		//         subdivide serait fausse.
		//         POUR LA MESURER : partir d'un cube (6 faces), appliquer
		//         `loopcut:n` pour n = 1..5 sur une selection connue, relever
		//         faces/sommets/aretes avant et apres a chaque n. Verifier que
		//         l'ecart est lineaire en n ; s'il ne l'est pas, la loi depend
		//         d'autre chose et il faut le dire plutot que d'ajuster.
		//      3. `extrude[:individuelles]`  **NON MESURE**. Extruder s faces
		//         selectionnees ajoute une couronne par face : l'ordre de
		//         grandeur est additif en s, pas multiplicatif. Le drapeau
		//         « faces individuelles » change le resultat (couronne par face
		//         contre couronne du bord commun) : DEUX lois, pas une.
		//         POUR LA MESURER : selection de s faces pour s = 1, 2, 6, 24,
		//         chaque fois avec le drapeau a 0 PUIS a 1. Huit relevés.
		//      4. `inset[:individuel[:profondeur]]`  **NON MESURE**. Meme forme
		//         qu'extrude (additif en s, deux lois selon `individuel`). La
		//         profondeur ne devrait pas changer les COMPTES, seulement les
		//         positions -- **a verifier, c'est une supposition**.
		//      5. `bevel[:largeur[:segments]]`  **MESURE le 20/09**, et il etait
		//         bien le plus gros risque des quatre. DEUX regimes, EXACTS sur
		//         les dix relevés, sans le moindre residu -- des lois LUES, pas
		//         une courbe ajustee sur ses propres points :
		//             segments == 1  ->  faces ajoutees = **E + V**
		//             segments >= 2  ->  faces ajoutees = **3 x E x segments**
		//         (E = aretes, V = sommets, comptes AVANT l'operation.)
		//         La rupture entre les deux regimes n'est pas une anomalie :
		//         `NkEditMesh.h` la documente lui-meme -- 1 = chanfrein PLAT (une
		//         bande de faces), N > 1 = ARRONDI (N bandes). Deux topologies,
		//         deux lois, et c'est pourquoi une formule unique aurait menti.
		//         ⚠️ LE RISQUE EST CONFIRME PAR LE CHIFFRE : sur 48 aretes,
		//            `bevel:0.05:16` porte 24 faces a 2 328 -- **x97**. En theorie
		//            `subdivide:10` fait pire, mais bevel y arrive avec des
		//            parametres qu'un utilisateur tape sans y penser.
		//         MESURE PAR : `NKEditMeshHarness --loi-bevel` -- le harnais sans
		//         viseur, donc hors du verrou des deux modes Edition. Il rend
		//         AVANT les batteries comparees et n'ecrit RIEN dans la baseline :
		//         `--check` reste octet pour octet ce qu'il etait.
		//         ⚠️ CONDITION DE VALIDITE, ETROITE ET ECRITE : les DEUX maillages
		//            testes sont FERMES et tout-quads. Rien n'est mesure sur un
		//            maillage a BORD -- une arete de bord ne porte qu'une face et
		//            ne peut pas engendrer la meme bande -- ni sur des triangles.
		//            *Un chiffre voyage sans sa condition* : celle-ci reste
		//            collee a lui, et la prediction se DESACTIVE d'elle-meme quand
		//            E et V ne sont pas fournis (voir `NkIaPasSur`).
		//
		//  (B) CEUX QUI FONT DECROITRE -> le plafond de FACES n'est pas le
		//      risque ; c'est le plafond de TOURS qui garde (une boucle qui
		//      n'avance pas). Aucune loi de croissance n'est requise pour eux.
		//      6. `delete`   attendu decroissant, **NON MESURE**.
		//      7. `dissolve` attendu decroissant, **NON MESURE**.
		//      ⚠️ « Attendu » est le mot juste : je n'ai pas mesure que dissoudre
		//         ne peut jamais AUGMENTER un compte. La garde ne repose donc pas
		//         sur cette attente -- elle lit le SENS DU PREDICAT, qui est une
		//         donnee, pas une supposition (cf. `NkIaPasSur`).
		//
		//  ⚠️ CE QUI RENDRAIT CES MESURES POSSIBLES N'EXISTE PAS ENCORE. Les
		//     compteurs `Demo3DHostStats` ne sont renseignes QUE lorsque le
		//     VISEUR est en mode Edition (`st->editMode`), et le crochet
		//     `NK_EDIT_MODE` pose le mode du SHELL (`st.mode`) -- deux etats
		//     distincts. `NK_EDIT_PICK` exige, lui, `Demo3DHostInEditMode()` deja
		//     vrai : aucun crochet n'amorce donc l'etat. **Les six relevés
		//     ci-dessus sont a la merci de ce chainon**, et c'est la meme absence
		//     qui empeche `AI EFFET` de temoigner depuis le 19/09.
		//
		//  CONDITION DE RETRAIT DE CETTE LISTE : chaque ligne disparait le jour
		//  ou sa loi est mesuree ET ecrite dans `NkIaPasSur`. La liste entiere
		//  disparait quand les cinq croissants sont predits exactement.

		/// Les SIX quantites, et elles existent toutes deja dans l'hote.
		/// Aucune API nouvelle : `Demo3DHostStats` en donne quatre,
		/// `Demo3DHostObjectCount` et `Demo3DHostEditSelCount` les deux autres.
		enum class NkIaQuantite : uint8 { Faces = 0, Sommets, Aretes, Triangles, Objets, Selection, Aucune };

		inline NkIaQuantite NkIaQuantiteDuNom(const char *n) {
			if (!n)
				return NkIaQuantite::Aucune;
			struct { const char *nom; NkIaQuantite q; } kT[] = {
				{"faces", NkIaQuantite::Faces},		  {"sommets", NkIaQuantite::Sommets},
				{"aretes", NkIaQuantite::Aretes},	  {"triangles", NkIaQuantite::Triangles},
				{"objets", NkIaQuantite::Objets},	  {"selection", NkIaQuantite::Selection},
			};
			for (uint32 i = 0; i < 6u; ++i) {
				const char *a = kT[i].nom;
				const char *b = n;
				while (*a && *a == *b) { ++a; ++b; }
				if (!*a && !*b)
					return kT[i].q;
			}
			return NkIaQuantite::Aucune;
		}

		inline const char *NkIaQuantiteNom(NkIaQuantite q) {
			switch (q) {
				case NkIaQuantite::Faces: return "faces";
				case NkIaQuantite::Sommets: return "sommets";
				case NkIaQuantite::Aretes: return "aretes";
				case NkIaQuantite::Triangles: return "triangles";
				case NkIaQuantite::Objets: return "objets";
				case NkIaQuantite::Selection: return "elements selectionnes";
				default: return "?";
			}
		}

		// ── LES SEPT VERBES, ET C'EST UNE BORNE DU DESIGN ─────────────────────
		// ⚠️ « fais X jusqu'a Y » n'a de sens que pour un verbe qui DEPLACE une
		//    quantite mesurable. Sur les 26 du contrat, **7** le font : c'est leur
		//    role. Les 19 autres basculent un etat (submode*, toggleedit,
		//    togglexray), cadrent une vue (view*, frameall), pilotent une modale
		//    (modal*, 6) ou rejouent l'historique (undo/redo -- ils bougent bien
		//    les compteurs, mais par EFFET DE BORD, et boucler dessus n'a pas de
		//    sens productif).
		// ⚠️ LA PORTE REFUSE NOMMEMENT pour les 19 autres. Elle ne se rabat
		//    SURTOUT PAS sur un tour unique : *une boucle qui fait toujours
		//    exactement un tour est une boucle qui ment sur ce qu'elle est*, et
		//    l'utilisateur croirait avoir boucle.
		inline bool NkIaVerbeBouclable(const char *verbe) {
			if (!verbe)
				return false;
			static const char *kSept[] = {"subdivide", "loopcut", "extrude",
										  "inset",	   "bevel",	  "delete", "dissolve"};
			for (uint32 i = 0; i < 7u; ++i) {
				const char *a = kSept[i];
				const char *b = verbe;
				while (*a && *a == *b) { ++a; ++b; }
				// le verbe peut porter ses parametres : « subdivide:2 » compte
				if (!*a && (*b == 0 || *b == ':'))
					return true;
			}
			return false;
		}

		/// DETACHE le predicat du verbe quand le modele les a COLLES.
		/// ⚠️ DEFAUT OBSERVE, PAS PREVU. `qwen2.5:7b` a rendu, sur une seule
		///    ligne, `delete:jusqua:objets:moins:2` -- la faute exacte que le banc
		///    du 20/09 avait deja nommee chez lui (« les deux-points ne separent
		///    JAMAIS deux commandes », il l'ignore). Sans cette coupe, le verbe
		///    ARME de la boucle etait la chaine entiere : elle passait
		///    `NkVerbeTrouve` (un verbe a le droit de porter des parametres), puis
		///    partait au pont VINGT-CINQ FOIS avec quatre parametres parasites.
		///    Claude, lui, rend deux lignes propres -- **et c'est precisement
		///    pourquoi la garde doit exister : elle protege du modele qu'on n'a
		///    pas choisi, pas de celui qu'on a mesure.**
		inline void NkIaCouperPredicat(char *verbe) {
			if (!verbe)
				return;
			for (char *c = verbe; *c; ++c) {
				if (*c != ':')
					continue;
				const char *m = ":jusqua";
				const char *p = c;
				while (*m && *m == *p) { ++m; ++p; }
				if (!*m) {
					*c = 0; // le verbe s'arrete avant la condition
					return;
				}
			}
		}

		/// Cherche le jeton `jusqua:...` N'IMPORTE OU dans la reponse.
		/// ⚠️ ON NE COMPTE PAS LES LIGNES. Prendre « la seconde ligne » supposerait
		///    que la premiere est le verbe et qu'il n'y a ni ligne vide ni ornement
		///    -- une hypothese sur la mise en forme d'un modele, c'est-a-dire la
		///    chose qui bouge le plus. On cherche le motif, qui, lui, est a nous.
		inline bool NkIaTrouverPredicat(const char *reponse, char *dst, uint32 taille) {
			if (!dst || taille == 0)
				return false;
			dst[0] = 0;
			if (!reponse)
				return false;
			for (const char *c = reponse; *c; ++c) {
				const char *m = "jusqua:";
				const char *p = c;
				while (*m && *m == *p) { ++m; ++p; }
				if (*m)
					continue;
				uint32 n = 0;
				for (const char *q = c; *q && n + 1u < taille; ++q) {
					const char x = (*q >= 'A' && *q <= 'Z') ? (char)(*q - 'A' + 'a') : *q;
					const bool ok = (x >= 'a' && x <= 'z') || (x >= '0' && x <= '9') || x == ':' ||
									x == '.' || x == '-' || x == '_';
					if (!ok)
						break;
					dst[n++] = x;
				}
				dst[n] = 0;
				return n > 0;
			}
			return false;
		}

		struct NkIaPredicat {
				NkIaQuantite quantite = NkIaQuantite::Aucune;
				bool plus = true; ///< vrai = « depasser », faux = « descendre sous »
				int32 seuil = 0;
				bool valide = false;
		};

		/// Lit `jusqua:quantite:sens:seuil`. Rend `valide=false` pour `aucune`,
		/// pour un jeton malforme et pour une quantite inconnue -- TROIS cas que
		/// l'appelant distingue par ce qu'il a lu, pas par un booleen unique.
		inline NkIaPredicat NkIaLirePredicat(const char *jeton) {
			NkIaPredicat p;
			if (!jeton)
				return p;
			// « jusqua: »
			const char *c = jeton;
			const char *m = "jusqua:";
			while (*m && *m == *c) { ++m; ++c; }
			if (*m)
				return p; // pas un predicat (c'est `aucune`, ou autre chose)
			char nom[16];
			uint32 n = 0;
			while (*c && *c != ':' && n + 1u < sizeof(nom))
				nom[n++] = *c++;
			nom[n] = 0;
			if (*c != ':')
				return p;
			++c;
			p.quantite = NkIaQuantiteDuNom(nom);
			if (p.quantite == NkIaQuantite::Aucune)
				return p;
			if (*c == 'p')
				p.plus = true;
			else if (*c == 'm')
				p.plus = false;
			else
				return p;
			while (*c && *c != ':')
				++c;
			if (*c != ':')
				return p;
			++c;
			// ⚠️ PAS D'atof ICI. En fr-FR, `atof("0.2")` rend 0.0 -- le depot l'a
			//    deja paye. Un seuil est un ENTIER de comptage : on le lit chiffre
			//    a chiffre, sans libc et sans locale.
			int32 v = 0;
			bool chiffre = false;
			for (; *c >= '0' && *c <= '9'; ++c) {
				v = v * 10 + (int32)(*c - '0');
				chiffre = true;
				if (v > 100000000)
					break; // borne de lecture : au-dela, le seuil n'a plus de sens
			}
			if (!chiffre)
				return p;
			p.seuil = v;
			p.valide = true;
			return p;
		}

		/// Le predicat est-il DEJA satisfait par l'etat qu'on vient de LIRE ?
		/// ⚠️ `valeur` vient des compteurs de l'hote, jamais d'une prediction.
		inline bool NkIaPredicatSatisfait(const NkIaPredicat &p, int32 valeur) {
			return p.plus ? (valeur > p.seuil) : (valeur < p.seuil);
		}

		// ── LE PLAFOND ABSOLU, ET IL SE VERIFIE **AVANT** LE PAS ──────────────
		// ⚠️ J'AVAIS ECRIT QUE CE GARDE-FOU DEVENAIT INUTILE. C'ETAIT FAUX, et la
		//    correction est du coordinateur : sortir le modele de la boucle borne
		//    le depassement « a un pas » -- mais **un pas de `subdivide` est
		//    MULTIPLICATIF**. Mesure du 17/09 : 6 -> 24 pour `subdivide`, 6 -> 384
		//    pour `subdivide:3`, soit x4^k. « Borne par un pas » veut donc dire
		//    « borne par 4x le seuil », pas « borne ». Et la loi n'est pas la meme
		//    selon le verbe : `extrude` n'ajoute pas comme `subdivide` multiplie.
		//    **Deux verbes, deux lois de croissance, une seule phrase « un pas ».**
		//
		// ⚠️ ET LA VERIFICATION EST DECALEE D'UN CRAN : AVANT d'appliquer, pas
		//    apres. Apres coup le maillage est deja a 400 000 faces et la fenetre
		//    est deja partie ; le constat arrive trop tard pour servir a quoi que
		//    ce soit.
		//
		// ⚠️ LA MENACE A CHANGE DE CAMP, LE GARDE-FOU RESTE. Il ne protege plus du
		//    MODELE qui choisissait `subdivide:10` -- le modele n'est plus dans la
		//    boucle. Il protege du PAS LUI-MEME. Retirer un garde-fou parce que la
		//    menace a change de camp est une faute que ce depot a deja payee.
		//
		// CONDITION DE RETRAIT : le jour ou chaque verbe bouclable publie sa loi
		// de croissance MESUREE, cette borne peut devenir une prediction exacte
		// par verbe. Tant que UNE SEULE loi est mesuree (subdivide), elle reste.
		inline int32 NkIaPlafondFaces() {
			static int32 sPlafond = -1;
			if (sPlafond < 0) {
				sPlafond = 2000000; // 2 M faces : au-dela, l'interactivite est perdue
				if (const char *v = std::getenv("NK_AI_BOUCLE_PLAFOND")) {
					int32 x = 0;
					for (const char *c = v; *c >= '0' && *c <= '9'; ++c)
						x = x * 10 + (int32)(*c - '0');
					if (x > 0)
						sPlafond = x;
				}
			}
			return sPlafond;
		}

		/// Le pas est-il SUR a appliquer ? `motif` recoit la raison du refus.
		/// ⚠️ UNE SEULE LOI DE CROISSANCE EST MESUREE, ET JE NE PREDIS QUE
		///    CELLE-LA. Pour `subdivide:k`, le resultat est exactement
		///    `faces * 4^k` (mesure du 17/09). Pour les six autres verbes
		///    bouclables, **je n'ai aucune loi mesuree** : je refuse donc de
		///    predire, et j'applique une regle conservatrice -- ne pas faire un
		///    pas de plus quand on est deja au quart du plafond. C'est une
		///    HYPOTHESE, elle est ecrite ici, et elle se remplace par une mesure.
		/// `versLeHaut` est le SENS DU PREDICAT (`plus` = vrai). Il n'est pas
		/// decoratif : c'est la seule chose qu'on sache de la direction de la
		/// boucle sans avoir mesure la loi du verbe. Voir le corps.
		///
		/// `aretes` et `sommets` servent a PREDIRE `bevel`, dont la loi a ete
		/// mesuree le 20/09 (voir la liste en tete). ⚠️ A ZERO -- leur defaut --
		/// LA PREDICTION SE DESACTIVE et l'on retombe sur la regle du quart.
		/// C'est voulu : un appelant qui ne fournit pas ces comptes n'obtient pas
		/// une prediction batie sur des zeros, il obtient la garde prudente.
		/// ⚠️ AUCUN APPELANT NE LES PASSE ENCORE : `main.cpp` lit pourtant E et V
		///    a deux lignes du site (`bv`, `be`), mais ce fichier est en cours de
		///    migration par un autre chantier et je n'y touche pas. **La branche
		///    bevel de cette fonction n'est donc pas exercee aujourd'hui** -- ne
		///    pas la lire comme eprouvee ; c'est un ajout d'une ligne au site
		///    d'appel, le jour ou la migration est fusionnee.
		inline bool NkIaPasSur(const char *verbe, int32 facesActuelles, bool versLeHaut,
							   char *motif, uint32 taille, int32 aretes = 0, int32 sommets = 0) {
			const int32 plafond = NkIaPlafondFaces();
			int32 prevu = facesActuelles;
			bool predit = false;
			const char *p = verbe ? verbe : "";
			const char *m = "subdivide";
			const char *c = p;
			while (*m && *m == *c) { ++m; ++c; }
			if (!*m && (*c == 0 || *c == ':')) {
				int32 k = 1;
				if (*c == ':') {
					++c;
					int32 x = 0;
					bool d = false;
					for (; *c >= '0' && *c <= '9'; ++c) { x = x * 10 + (int32)(*c - '0'); d = true; }
					if (d && x >= 1 && x <= 10)
						k = x;
				}
				prevu = facesActuelles;
				for (int32 i = 0; i < k; ++i) {
					if (prevu > plafond) break;
					prevu *= 4; // LOI MESUREE le 17/09 : x4 par coupe
				}
				predit = true;
			}
			// ── `bevel` : DEUX REGIMES, MESURES le 20/09 ──────────────────────
			// ⚠️ ELLE N'EST TENTEE QUE SI E ET V SONT FOURNIS. A zero, on ne
			//    predit pas avec des zeros : on laisse la regle du quart faire son
			//    travail. *Une prediction batie sur une absence est pire que pas
			//    de prediction : elle a l'air d'un chiffre.*
			if (!predit && aretes > 0 && sommets > 0) {
				const char *mb = "bevel";
				const char *cb = p;
				while (*mb && *mb == *cb) { ++mb; ++cb; }
				if (!*mb && (*cb == 0 || *cb == ':')) {
					// `bevel[:largeur[:segments]]` -- segments est le SECOND
					// parametre. La largeur ne change pas les comptes (mesure).
					int32 segments = 1;
					if (*cb == ':') {
						++cb;
						while (*cb && *cb != ':') // on saute la largeur
							++cb;
						if (*cb == ':') {
							++cb;
							int32 x = 0;
							bool d = false;
							for (; *cb >= '0' && *cb <= '9'; ++cb) {
								x = x * 10 + (int32)(*cb - '0');
								d = true;
							}
							if (d && x >= 1 && x <= 16)
								segments = x;
						}
					}
					// segments == 1 -> E + V ; segments >= 2 -> 3 x E x segments.
					const int32 ajout =
						(segments == 1) ? (aretes + sommets) : (3 * aretes * segments);
					prevu = facesActuelles + ajout;
					predit = true;
				}
			}
			if (predit && prevu > plafond) {
				snprintf(motif, taille,
						 "Boucle arretee : le prochain pas porterait le maillage a environ %d faces, "
						 "au-dela du plafond de %d (NK_AI_BOUCLE_PLAFOND).",
						 prevu, plafond);
				return false;
			}
			if (!predit) {
				// ── LA DIRECTION VIENT DU PREDICAT, PAS D'UNE LOI DEVINEE ──────
				// ⚠️ DEFAUT TROUVE EN DRESSANT LA LISTE DES LOIS NON MESUREES, ET
				//    IL ETAIT DANS CE FICHIER. La regle du quart s'appliquait a
				//    TOUS les verbes non predits -- donc aussi a `delete` et
				//    `dissolve`. Une boucle « supprime jusqu'a moins de N faces »
				//    lancee sur un maillage deja au-dessus du quart du plafond
				//    etait REFUSEE, alors qu'elle ne pouvait que faire DESCENDRE le
				//    compte. *Un garde-fou qui refuse le geste qui reduit le risque
				//    protege contre lui-meme.*
				//
				// ⚠️ ET JE NE LE REPARE PAS EN DECRETANT « delete reduit ». Je ne
				//    l'ai pas mesure, et remplacer une hypothese par une autre ne
				//    m'avance pas. Je me sers de ce que je SAIS : la DIRECTION que
				//    l'utilisateur a demandee, qui est dans le predicat.
				//      `plus`  -> la boucle grossit par INTENTION : le plafond de
				//                 faces est le risque qui mord, regle du quart.
				//      `moins` -> la boucle reduit par INTENTION : le plafond de
				//                 faces n'est pas le risque, et c'est le plafond de
				//                 TOURS qui garde (il a d'ailleurs parle : « objets
				//                 = 86 n'a pas atteint 2 » apres 24 tours). On ne
				//                 refuse que si l'on est DEJA au-dela du plafond
				//                 absolu.
				//    ⚠️ Si le verbe ne va pas dans le sens demande, la boucle
				//       n'avance pas -- et c'est exactement la panne que le plafond
				//       de TOURS attrape, en la nommant.
				if (versLeHaut && facesActuelles > plafond / 4) {
					// L'HYPOTHESE, DITE A L'UTILISATEUR PLUTOT QUE CACHEE.
					snprintf(motif, taille,
							 "Boucle arretee a %d faces : la croissance de « %s » n'est pas "
							 "mesuree, on ne franchit pas le quart du plafond de %d a l'aveugle.",
							 facesActuelles, p, plafond);
					return false;
				}
				if (!versLeHaut && facesActuelles > plafond) {
					snprintf(motif, taille,
							 "Boucle arretee : le maillage est deja a %d faces, au-dela du "
							 "plafond de %d (NK_AI_BOUCLE_PLAFOND).",
							 facesActuelles, plafond);
					return false;
				}
			}
			return true;
		}

		/// L'ADDENDUM — les MEMES lignes que le banc P1/P2, plus la demande du
		/// verbe. ⚠️ CE N'EST PAS UN CINQUIEME CONSTRUCTEUR D'INVITE : il
		/// s'APPEND a `NkIaEcrireContratDansInvite`, qui reste l'autorite unique.
		inline void NkIaEcrireAddendumBoucle(char *dst, uint32 taille) {
			if (!dst)
				return;
			uint32 n = 0;
			while (n < taille && dst[n])
				++n;
			auto ajout = [&](const char *s) {
				while (*s && n + 1u < taille)
					dst[n++] = *s++;
				dst[n] = 0;
			};
			ajout("\nSi la demande dit de REPETER une operation JUSQU'A une condition CHIFFREE,\n");
			ajout("reponds en DEUX LIGNES :\n");
			ajout("ligne 1 : la commande a repeter (un verbe du tableau ci-dessus)\n");
			ajout("ligne 2 : jusqua:quantite:sens:seuil\n");
			ajout("quantite est l'un de : faces, sommets, aretes, triangles, objets, selection\n");
			ajout("sens est  plus  (depasser le seuil) ou  moins  (descendre sous le seuil)\n");
			ajout("Si la demande ne porte AUCUNE condition chiffree et mesurable, n'ecris pas\n");
			ajout("de seconde ligne.\n");
		}

		// ── LE DORSAL, ET IL EST UN REGLAGE ────────────────────────────────────
		// `NK_IA_CMD` porte le gabarit, deux trous `{invite}` et `{sortie}`.
		// Absent, on compose un defaut a partir de `NK_IA_PYTHON` (sinon
		// `python`) et du script livre. ⚠️ CE DEFAUT N'EST PAS UNE DEPENDANCE :
		// il rend l'application utilisable sans configuration, et il se remplace
		// par une variable le jour ou le dorsal du module arrive.
		struct NkIaCanal {
				converse::NkConverseBackendProcessus dorsal;
				// ── LE DORSAL DISTANT, A COTE ET JAMAIS A LA PLACE ────────────────
				// ⚠️ IL N'EST PAS CHOISI PAR DEFAUT, ET C'EST DELIBERE. Le local ne
				//    fait rien sortir de la machine ; celui-ci envoie l'invite chez
				//    Anthropic. Un dorsal distant qui s'imposerait « parce qu'il
				//    repond » ferait partir la premiere demande avant que Rodolf ait
				//    su qu'elle partait. C'est l'ONGLET qui le designe, donc un geste.
				converse::NkConverseBackendClaude claude;
				converse::NkEnvoiAsync envoi;
				bool pret = false;
				char motif[256] = {0};

				/// L'ONGLET -> LE DORSAL. Une seule fonction, et elle est la seule
				/// autorite : `main.cpp` lui demande au lieu de choisir de son cote.
				/// Deux endroits qui decident du dorsal auraient fini par ne pas
				/// designer le meme, et l'un des deux aurait eu raison en silence.
				/// Rend `nullptr` pour un onglet sans cablage -- ce qui est un refus
				/// nomme par `MotifDe`, pas une panne.
				converse::NkIConverseBackend *DorsalDe(int32 onglet) {
					if (onglet == 1)
						return claude.IsAvailable() ? (converse::NkIConverseBackend *)&claude : nullptr;
					if (onglet == 0)
						return pret ? (converse::NkIConverseBackend *)&dorsal : nullptr;
					return nullptr; // l'onglet Ollama n'a pas de cablage dans le modeleur
				}

				/// LE ZERO, ET IL DIT LEQUEL. Trois onglets, trois raisons possibles
				/// de ne rien pouvoir faire, et elles se reparent a trois endroits
				/// differents. ⚠️ Le conseil passe DEVANT le diagnostic : cette
				/// chaine s'affiche dans une ligne qui tronque, et c'est le geste qui
				/// repare qu'il ne faut pas perdre a la coupure.
				void MotifDe(int32 onglet, char *dst, uint32 taille) const {
					if (!dst || taille == 0)
						return;
					dst[0] = 0;
					if (onglet == 1) {
						NkString quoiFaire;
						if (converse::NkClaudeDiagnostic(claude.compte, quoiFaire) !=
							converse::NkClaudeEtat::Pret)
							snprintf(dst, taille, "%s", quoiFaire.Data() ? quoiFaire.Data() : "");
						return;
					}
					if (onglet == 0) {
						if (!pret)
							snprintf(dst, taille, "%s", motif);
						return;
					}
					snprintf(dst, taille,
							 "Choisissez Local ou Claude : l'onglet Ollama n'est pas cable au "
							 "modeleur (le transport existe, le cablage non).");
				}

				/// LA LIGNE D'ETAT DE L'ONGLET. Elle dit le dorsal ET ce qu'il coute
				/// -- et pour le distant, elle dit d'abord QUE CA SORT.
				void LigneEtat(int32 onglet, char *dst, uint32 taille) const {
					if (!dst || taille == 0)
						return;
					if (onglet == 1) {
						NkString quoiFaire;
						if (converse::NkClaudeDiagnostic(claude.compte, quoiFaire) !=
							converse::NkClaudeEtat::Pret) {
							snprintf(dst, taille, "claude · %s",
									 quoiFaire.Data() ? quoiFaire.Data() : "");
							return;
						}
						// ⚠️ LE CHIFFRE, PAS SEULEMENT L'AVERTISSEMENT. « des donnees
						//    partent » se survole ; « 2 431 octets sont partis » se lit.
						//    Il vaut 0 tant qu'aucune demande n'a ete envoyee, et c'est
						//    vrai : rien n'est encore sorti.
						snprintf(dst, taille, "claude · %s modele %s · %u octets envoyes",
								 converse::NkClaudeAvertissement(),
								 claude.modele.Data() ? claude.modele.Data() : "?",
								 (unsigned)claude.DerniereInvite().Length());
						return;
					}
					if (onglet == 0) {
						snprintf(dst, taille,
								 pret ? "local · rien ne quitte cette machine · %s"
									  : "local · indisponible · %s",
								 dorsal.Name() ? dorsal.Name() : "?");
						return;
					}
					snprintf(dst, taille, "ollama · aucun cablage dans le modeleur");
				}

				void Preparer(const char *racine) {
					dorsal.nom = NkString("assistant");
					// ── LE DISTANT : SES FICHIERS DE TRAVAIL, ET SON COMPTE ───────
					// Meme regle que le local : dans `logs/`, pas a la racine.
					// ⚠️ LE COMPTE EST UN REGLAGE, ET IL EST VIDE PAR DEFAUT -- le
					//    dossier de configuration ordinaire du CLI. `NK_CLAUDE_COMPTE`
					//    vise un compte de NKCode (~/.nkcode/accounts/<nom>). Aucune
					//    cle n'est lue nulle part : on ne pose qu'un CHEMIN.
					claude.invitePath = NkString("logs/nkclaude_invite.txt");
					claude.sortiePath = NkString("logs/nkclaude_reponse.txt");
					claude.scriptPath = NkString("logs/nkclaude_lance.cmd");
					if (const char *c = std::getenv("NK_CLAUDE_COMPTE"))
						if (*c)
							claude.compte = NkString(c);
					if (const char *m = std::getenv("NK_CLAUDE_MODELE"))
						if (*m)
							claude.modele = NkString(m);
					// ⚠️ DANS `logs/`, PAS A LA RACINE. Le dorsal ecrit deux fichiers de
					//    travail a CHAQUE appel ; poses dans le repertoire courant, ils
					//    salissent le dossier de projet de Rodolf et reapparaissent apres
					//    chaque nettoyage. `logs/` existe deja et c'est la que vivent les
					//    traces de l'application.
					dorsal.invitePath = NkString("logs/nk3dmodeler_invite.txt");
					dorsal.sortiePath = NkString("logs/nk3dmodeler_reponse.txt");
					if (const char *g = std::getenv("NK_IA_CMD")) {
						if (*g)
							dorsal.gabarit = NkString(g);
					}
					if (dorsal.gabarit.Length() == 0) {
						const char *py = std::getenv("NK_IA_PYTHON");
						char buf[512];
						snprintf(buf, sizeof(buf), "%s \"%s%sTools/Genia/ia_verbe.py\" \"{invite}\" \"{sortie}\"",
								 (py && *py) ? py : "python", racine ? racine : "",
								 (racine && *racine) ? "/" : "");
						dorsal.gabarit = NkString(buf);
					}
					pret = dorsal.IsAvailable();
					if (!pret)
						snprintf(motif, sizeof(motif),
								 "Aucun dorsal de conversation : posez NK_IA_CMD.");
				}
		};

	} // namespace nk3d
} // namespace nkentseu
