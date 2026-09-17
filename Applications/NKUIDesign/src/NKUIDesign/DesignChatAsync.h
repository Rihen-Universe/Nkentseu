#pragma once
// -----------------------------------------------------------------------------
// @File    DesignChatAsync.h
// @Brief   L'IA REPOND PENDANT QUE LA FENETRE VIT — un fil, un objet partage,
//          et UNE regle de partage ecrite avant la premiere ligne de code.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CA FERME
// =============================================================================
//  `NkDesignConversation::Envoyer` appelle `NkIDesignBackend::Complete`, qui
//  BLOQUE. Avec le dorsal fichier c'est instantane ; avec le dorsal par
//  processus qui charge un modele local, la mesure du 14/09 donne
//  **12,3 s de chargement + 150,3 s de prefill + 818 ms par jeton** — une course
//  complete a dure **7 min 24 s, fenetre figee**. Ce n'est pas un plantage, mais
//  c'est inutilisable, et Rodolf l'a redemande deux fois.
//
// =============================================================================
//  LA REGLE DE PARTAGE — ecrite AVANT le code, et il n'y en a qu'une
// =============================================================================
//  Les deux fils ne partagent QU'UN objet, `NkTacheIA`, et **chaque champ a UN
//  SEUL ECRIVAIN**. C'est toute la discipline ; tout le reste en decoule.
//
//      champ     ecrit par        lu par           la lecture est sure...
//      ------    -------------    -------------    --------------------------
//      etat      le TRAVAILLEUR   l'INTERFACE      toujours (atomique)
//      sortie    le TRAVAILLEUR   l'INTERFACE      SEULEMENT si etat != EnCours
//      erreur    le TRAVAILLEUR   l'INTERFACE      SEULEMENT si etat != EnCours
//      annule    l'INTERFACE      le TRAVAILLEUR   toujours (atomique)
//      invite    l'INTERFACE      le TRAVAILLEUR   ecrit AVANT le lancement
//      dorsal    l'INTERFACE      le TRAVAILLEUR   ecrit AVANT le lancement
//
//  ⚠️ `etat` EST LA BARRIERE, et c'est la seule piece subtile. Le travailleur
//     ecrit `sortie` **puis** pose `etat` en RELEASE ; l'interface lit `etat` en
//     ACQUIRE **puis** lit `sortie`. La paire release/acquire garantit que tout
//     ce qui a ete ecrit avant le Store est visible apres le Load. Sans elle,
//     l'interface pourrait voir `etat = Finie` et une `sortie` a moitie ecrite —
//     un defaut qui ne se reproduit pas, ne se journalise pas, et se corrige a
//     coups de « ca remarche ».
//
//  ⚠️ AUCUN VERROU, ET C'EST UN CHOIX MOTIVE. Un mutex pris a chaque image pour
//     lire un booleen serait une contention gratuite — mais surtout il ne dirait
//     PAS quelle donnee est prete. La barriere, elle, le dit.
//
//  ⚠️ LA PROPRIETE EST PARTAGEE, PAR UN COMPTEUR A DEUX. C'est ce qui rend
//     l'annulation SURE alors que `Complete()` NE PEUT PAS ETRE INTERROMPU : il
//     est, chez le dorsal par processus, un `WaitForSingleObject(INFINITE)`.
//     Annuler ne tue donc rien :
//        1. l'interface pose `annule`, LACHE sa part et ne touche plus jamais
//           l'objet — pas meme pour le lire ;
//        2. le travailleur finit sa generation (on ne peut pas l'en empecher),
//           voit `annule`, **jette sa reponse**, lache sa part ;
//        3. le dernier des deux detruit.
//     C'est ce qui tient la promesse « apres annulation, pas de reponse qui
//     arrive apres coup dans la conversation suivante » : la reponse n'est pas
//     ignoree plus tard, elle n'est **jamais posee**.
//
// =============================================================================
//  DECISION DE CONCEPTION DU 2026-09-14 — PAS DE FLUX DE JETONS DANS LE CONTRAT
// =============================================================================
//  ⚠️ CE N'EST PAS UNE LIMITE SUBIE, C'EST UN CHOIX, ET IL EST DATE. Une
//     contrainte qu'on s'impose sans l'expliquer finit par etre prise pour un
//     oubli, et le premier qui passe la « repare ».
//
//  CE QU'ON REFUSE : ajouter a `NkIDesignBackend` un rappel de progression, du
//  genre `void (*surJeton)(const char*, void*)`.
//
//  POURQUOI : le contrat est ce qui rend le modele REMPLACABLE. Rodolf a pose la
//  trajectoire pour les deux generateurs — « rassure-toi qu'il va a la longue

// ═══════════════════════════════════════════════════════════════════════
//  CE FICHIER A DEMENAGE — il est desormais un RELAIS
// ═══════════════════════════════════════════════════════════════════════
//  Son contenu vit dans `Kernel/System/NKConverse`. La mesure qui l'a
//  autorise : ZERO occurrence du document dans ce fichier. Il etait deja
//  neutre, il ne le savait pas.
//
//  L'appel non bloquant declare desormais NKThreading et NKTime chez lui
//
//  ⚠️ LE FICHIER N'EST PAS SUPPRIME, ET C'EST LA REGLE DE LA MAISON :
//     un en-tete vide dit une intention, et le defaut n'est pas le
//     fichier mais le mauvais domicile. Les appelants d'ici continuent
//     d'ecrire les noms d'ici, sans une ligne de changement.
// ═══════════════════════════════════════════════════════════════════════

#include "NKConverse/NkConverseChatAsync.h"

namespace nkuidesign {

	using NkEtatTache = nkentseu::converse::NkEtatTache;
	using NkTacheIA = nkentseu::converse::NkTacheIA;
	using NkEnvoiAsync = nkentseu::converse::NkEnvoiAsync;
	// `NkDorsalLent` est un dorsal de MESURE (il repond lentement, expres) :
	// il descend avec le reste parce qu'un module qui ne fournit pas de quoi
	// eprouver son propre asynchronisme oblige chaque consommateur a en
	// reecrire un -- et c'est exactement le motif qui a produit trois copies.
	using NkDorsalLent = nkentseu::converse::NkDorsalLent;

} // namespace nkuidesign
