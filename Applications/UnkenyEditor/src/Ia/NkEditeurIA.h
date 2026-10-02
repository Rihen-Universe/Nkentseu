//
// NkEditeurIA.h
// =============================================================================
// Description :
//   L'IA INTEGREE d'UnkenyEditor (R18 de la feuille de route, document 05) :
//   le panneau IA du kit ancre a droite, un modele BRANCHE (Ollama, serveur
//   compatible OpenAI, Claude par l'API ou le CLI, modele local), qui COMPREND
//   Unkeny des sa premiere connexion et AGIT sur l'editeur par des OUTILS.
//
// Caracteristiques :
//   - « COMPREND DES LA CONNEXION » : le message systeme est ENGENDRE par
//     l'editeur a chaque tour (NkEditeurIADescription) -- les outils, le
//     catalogue des acteurs, les composants, les types d'assets, les commandes,
//     les noeuds Blueprint, la scene ouverte et sa selection. Aucun texte fige :
//     un acteur ajoute au moteur apparait a l'IA sans une ligne ici.
//   - « AGIT SUR TOUT » : chaque outil est une commande de l'editeur
//     (NkEditeurIAOutils.cpp) -- lire la scene, creer / modifier / supprimer
//     une entite, ses composants et TOUTES ses valeurs (par la forme de
//     sauvegarde de la scene), creer un dossier, ecrire un fichier, un script
//     C++, un Blueprint (noeuds et fils), le GDD, executer une commande.
//   - GARDE-FOUS :
//       * chaque action passe par l'HISTORIQUE de l'editeur
//         (NkEditeurRetenir) : Ctrl+Z la defait -- y compris l'ecriture d'un
//         fichier (le journal des fichiers de NkHistoriqueEditeur) ;
//       * une action IRREVERSIBLE (supprimer un fichier, en ECRASER un, une
//         nouvelle scene sur des modifications non enregistrees) attend la
//         CONFIRMATION dans le panneau : deux boutons, « Confirmer » et
//         « Refuser », sur le bloc de l'outil.
//   - Les modeles locaux sans appel d'outils natif ecrivent des blocs
//     `<outil nom="...">{...}</outil>` que l'editeur analyse (NKConverse).
//   - La conversation tourne sur un FIL DE TRAVAIL (NkEnvoiChatIA) : la fenetre
//     vit pendant qu'un modele local charge ses poids ; le texte arrive EN FLUX.
//
// Fichiers :
//   NkEditeurIA.cpp         la boucle (tours, outils, confirmation), le panneau,
//                           la fenetre des reglages
//   NkEditeurIAOutils.cpp   les outils, la description engendree, le journal
//   NkEditeurBancIA.cpp     le banc (faux serveur Ollama et OpenAI)
//   NkEditeurCapturesIA.cpp les captures hors ecran (--captures-ia=DOSSIER)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURIA_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURIA_H__

#include "Editeur/NkEditeurModele.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKConverse/NkConverseChatFlux.h"
#include "NKEditorKit/NkAiPanneau.h"
#include "NKEditorKit/NkAiReglagesVue.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;
		struct NkEditeurInterface;

		/// Ce qu'un outil a fait.
		struct NkResultatOutilIA {
				bool ok = false;
				/// Ce que le MODELE recoit (un objet JSON ou une phrase). Un refus y
				/// dit pourquoi : le modele corrige son appel au tour suivant.
				NkString texte;
				/// Ce que l'UTILISATEUR voit (« +1 entite « Caisse » en (0, 0) »).
				/// Vide : rien n'a change (une lecture).
				NkString effet;
				/// La scene ou un fichier a change : la photo prise AVANT reste dans
				/// l'historique (sinon elle est retiree -- un Ctrl+Z qui ne defait
				/// rien serait un mensonge de plus).
				bool modifie = false;
		};

		/// UN OUTIL de l'editeur, tel que le modele le voit.
		struct NkOutilEditeurIA {
				const char *nom = nullptr;
				const char *description = nullptr; ///< UNE phrase, ce que l'outil FAIT
				const char *schema = nullptr;	   ///< le schema JSON de ses parametres
				/// Toujours irreversible (supprimer un fichier). Les cas qui
				/// DEPENDENT des arguments (ecraser) sont decides par
				/// NkEditeurIAIrreversible.
				bool irreversible = false;
				/// Change la scene ou le disque : une photo est prise AVANT.
				bool modifie = true;
		};

		/// La table des outils (dans l'ordre ou le modele les lit).
		const NkVector<NkOutilEditeurIA> &NkEditeurIAOutils();
		/// Les memes, dans la forme de NKConverse (schemas et descriptions).
		void NkEditeurIAOutilsConverse(NkVector<converse::NkOutilIA> &sortie);
		/// LE MESSAGE SYSTEME, engendre maintenant depuis l'editeur.
		/// `outilsTexte` : le modele n'a pas d'outils natifs, la consigne des
		/// blocs <outil> et la liste des outils sont ajoutees.
		NkString NkEditeurIADescription(NkEditeurModele &m, bool outilsTexte);
		/// Cet appel doit-il attendre la CONFIRMATION ? `pourquoi` dit ce qui sera
		/// perdu (« ecrase Contenu/Scripts/Porte.cpp, 1 204 octets »).
		bool NkEditeurIAIrreversible(NkEditeurCadre &c, const converse::NkAppelOutil &appel, NkString &pourquoi);
		/// Execute un appel. Prend la PHOTO avant tout outil qui modifie (et la
		/// retire s'il n'a rien change). Jamais de confirmation ici : la boucle
		/// l'a deja demandee.
		NkResultatOutilIA NkEditeurIAExecuter(NkEditeurCadre &c, const converse::NkAppelOutil &appel);

		/// Une etape de la conversation en attente de l'utilisateur.
		enum class NkPhaseIA : uint8 { NK_REPOS = 0, NK_ATTENTE, NK_CONFIRMATION };

		/// L'IA de l'editeur : le panneau, les fournisseurs, la conversation.
		struct NkEditeurIA {
				editorkit::NkAiPanneau pan;

				// --- Les fournisseurs (NKConverse, partages avec NKCraft, NKCode) ---
				NkString dossier; ///< <AppData>/Nkentseu/IA, ou NK_IA_DOSSIER
				NkVector<converse::NkReglagesFournisseur> fournisseurs;
				/// Les modeles que chaque serveur a LISTES (meme indice).
				NkVector<NkVector<converse::NkConverseModeleInfo>> modeles;
				/// Le dernier verdict de chaque fournisseur (pret / motif du panneau).
				NkVector<NkString> motifs;
				/// Repasse en outils TEXTE apres un refus des outils natifs.
				NkVector<uint8> forceTexte;
				int32 actif = 0;

				// --- La conversation (une par chat du panneau) ----------------------
				NkVector<NkVector<converse::NkMessageIA>> echanges;
				converse::NkEnvoiChatIA envoi;
				NkPhaseIA phase = NkPhaseIA::NK_REPOS;
				bool outilsNatifs = true;	///< ceux du tour en vol
				uint32 blocFlux = 0u;		///< le bloc de prose qui grandit
				uint32 morceauxVus = 0u;	///< combien de morceaux de flux l'ecran a montres
				uint32 imagesFlux = 0u;		///< combien d'images ont montre un texte PARTIEL
				int32 etapes = 0;			///< tours outils -> modele dans la demande
				int32 etapesMax = 8;
				NkVector<converse::NkAppelOutil> aExecuter;
				uint32 prochain = 0u;
				NkVector<converse::NkMessageIA> resultats;
				uint32 blocConfirmation = 0u;
				NkString motifConfirmation;
				/// Le dernier effet : son bloc et la taille de l'historique juste
				/// apres -- « Annuler cette action » n'est actif que si RIEN n'a ete
				/// retenu depuis (sinon il annulerait le geste de quelqu'un d'autre).
				uint32 blocEffet = 0u;
				usize historiqueApresEffet = 0u;

				// --- La fenetre des reglages (NkAiReglagesVue du kit) ---------------
				editorkit::NkAiReglagesVue vue;
				bool reglagesOuverts = false;
				int32 vueIndex = 0; ///< le fournisseur que la fenetre montre
				converse::NkSondeIA sonde;
				int32 sondeIndex = -1; ///< le fournisseur que la sonde interroge
				bool listeDemandee = false; ///< la liste du fournisseur actif, une fois

				// --- Pour les bancs et le journal -----------------------------------
				uint32 actionsFaites = 0u;
				uint32 actionsRefusees = 0u;
				NkString dernierEffet;
				NkString derniereErreur;
				/// Faux : rien n'est ecrit chez l'utilisateur (bancs, captures).
				bool persister = true;
				/// La disposition du panneau telle qu'elle est ecrite sur le disque :
				/// on ne reecrit que ce qui a change.
				NkString dispositionEcrite;

				NkEditeurIA() = default;
				NkEditeurIA(const NkEditeurIA &) = delete;
				NkEditeurIA &operator=(const NkEditeurIA &) = delete;
		};

		/// Relit les fournisseurs (`dossier` vide : NkIaDossier()), declare le
		/// panneau, pose `m.ia`.
		void NkEditeurIADemarrer(NkEditeurIA &ia, NkEditeurModele &m, const char *dossier = nullptr);
		/// Une trame : le flux, la fin d'un tour, les outils, la sonde. A appeler
		/// a chaque image, panneau ouvert ou non (un tour continue panneau ferme).
		void NkEditeurIATrame(NkEditeurCadre &c);
		/// Envoie une demande (ce que fait Entree dans le composeur). false : un
		/// tour est deja en vol, ou aucun fournisseur.
		bool NkEditeurIAEnvoyer(NkEditeurCadre &c, const char *texte);
		/// La reponse a une confirmation en attente (le bouton du bloc).
		void NkEditeurIAConfirmer(NkEditeurCadre &c, bool accepte);
		/// Le fournisseur `i` devient celui du panneau ; son modele, `modele` si
		/// non nul.
		void NkEditeurIAChoisir(NkEditeurIA &ia, int32 i, const char *modele = nullptr);
		/// Ouvre la fenetre des reglages sur le fournisseur `i` (-1 : l'actif).
		void NkEditeurIAOuvrirReglages(NkEditeurIA &ia, int32 i = -1);

		/// Les actions du menu (NK_A_VOIR_IA, NK_A_REGLAGES_IA) ; false : pas a l'IA.
		bool NkEditeurActionIA(NkEditeurCadre &c, int32 action);
		/// Le panneau A PART, dans `ui.ia` (NkEditeurPlanifier) : deplie, ou replie
		/// en bande. Rien en onglet (le groupe Details | Monde le dessine).
		void NkEditeurDessinerIA(NkEditeurCadre &c);
		/// Le panneau EN ONGLET, dans `zone` (le contenu du groupe Details | Monde).
		void NkEditeurDessinerIADans(NkEditeurCadre &c, const nkgui::NkRect &zone);
		/// L'IA se voit-elle a l'ecran (onglet au premier plan, ou panneau deplie) ?
		bool NkEditeurIAVisible(const NkEditeurInterface &ui);
		/// La disposition RETENUE (<dossier IA>/unkeny_panneau.txt) : onglet ou a
		/// part, replie, ouvert, largeur. Lue au demarrage, ecrite quand elle change.
		void NkEditeurIALireDisposition(NkEditeurIA &ia, NkEditeurInterface &ui);
		void NkEditeurIARetenirDisposition(NkEditeurIA &ia, const NkEditeurInterface &ui);
		/// La fenetre des reglages, par-dessus tout (couche du dessus).
		void NkEditeurDessinerReglagesIA(NkEditeurCadre &c);
		/// La fenetre est-elle ouverte (elle est modale) ?
		bool NkEditeurReglagesIAOuverts(const NkEditeurModele &m);

		/// LES CONTRE-EPREUVES du banc : des MUTATIONS qui cassent un garde-fou, pour
		/// voir le temoin qui le surveille passer au rouge. Bits : 1 = pas de photo
		/// avant l'outil (Ctrl+Z ne defait plus) ; 2 = pas de confirmation (un
		/// irreversible s'execute tout de suite). 0 : la production.
		void NkEditeurIAMutation(uint32 bits);
		uint32 NkEditeurIAMutationActive();

		/// Le banc de l'IA (NkEditeurBancIA.cpp), lance par --selftest.
		int32 NkEditeurLancerBancIA();
		/// `--captures-ia=DOSSIER` : les captures hors ecran du panneau.
		int32 NkEditeurCapturesIA(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURIA_H__
