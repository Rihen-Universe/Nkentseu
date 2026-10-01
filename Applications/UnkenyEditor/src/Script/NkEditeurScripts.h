//
// NkEditeurScripts.h
// =============================================================================
// Description :
//   Les SCRIPTS dans l'editeur (document 01, § 4.7 et § 5) : le registre et
//   l'hote d'Unkeny branches sur la scene editee, la compilation des scripts
//   C++ du projet et leur RECHARGEMENT A CHAUD sans fermer l'editeur, le suivi
//   des Blueprints (.nkbp) du projet, et la page du graphe ouverte.
//
// Caracteristiques :
//   - Le projet : `<projet>/Contenu/Scripts/*.cpp` (les sources) et
//     `<projet>/Intermediaire/Scripts/` (le registre genere, la DLL et ses
//     copies par generation). Rien n'est ecrit dans le Contenu.
//   - A CHAQUE ENREGISTREMENT d'un .cpp (date ou taille changee, relevees deux
//     fois par seconde), l'editeur regenere le registre des classes (il lit les
//     NK_UNKENY_CLASSE des sources), lance clang++ SANS bloquer la trame, et a
//     la fin : succes -> NkModulesCpp::Recharger (les instances en jeu sont
//     migrees, l'etat garde) ; echec -> les erreurs au Journal, « fichier:ligne:
//     colonne: error: ... », relatives au projet ; L'ANCIENNE VERSION RESTE.
//   - Le compilateur : clang++ du PATH, sinon C:/msys64/ucrt64/bin (la chaine
//     clang-mingw du depot) ; l'en-tete de la table vient du depot
//     (Engine/Unkeny/src). Sans l'un ou l'autre, le Journal le DIT.
//   - Un .nkbp du projet modifie sur le disque est RELU (la page du graphe le
//     recompile et l'enregistre elle-meme) : pendant JEU, l'hote migre, les
//     variables restent (elles sont dans le composant, par nom).
//   - « Arreter » : l'hote detruit ses instances a la trame qui suit le retour
//     en EDITION (avant toute decharge de DLL : NkModulesCpp migre d'abord).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTS_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTS_H__

#include "Editeur/NkEditeurModele.h"
#include "Livraison/NkEditeurProcessus.h"
#include "Script/NkEditeurGraphe.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Unkeny/Script/NkUnkenyModulesCpp.h"
#include "Unkeny/Script/NkUnkenyScripts.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurInterface;

		/// Le dossier des scripts dans le Contenu, et celui des produits.
		constexpr const char *NK_SCRIPTS_DOSSIER = "Contenu/Scripts";
		constexpr const char *NK_SCRIPTS_INTERMEDIAIRE = "Intermediaire/Scripts";

		/// Une erreur de compilation, deja relative au projet.
		struct NkErreurScriptCpp {
				NkString fichier; ///< « Contenu/Scripts/Porte.cpp »
				int32 ligne = 0;
				int32 colonne = 0;
				NkString message;
		};

		enum class NkEtatCompilation : uint8 { NK_AUCUNE = 0, NK_EN_COURS, NK_REUSSIE, NK_ECHOUEE, NK_IMPOSSIBLE };

		struct NkEditeurScripts {
				unkeny::NkScripts2D registre;
				unkeny::NkHoteScripts2D hote;
				unkeny::NkModulesCpp modules;
				NkEditeurProcessus compilateur;

				// --- Le C++ du projet ------------------------------------------
				NkString projet;	   ///< le dossier du projet suivi (barre finale)
				NkVector<NkString> sources;
				NkVector<int64> empreintes; ///< date * 1e6 + taille, par source
				NkVector<NkString> classes; ///< lues dans les sources (NK_UNKENY_CLASSE)
				NkEtatCompilation etat = NkEtatCompilation::NK_AUCUNE;
				bool recompiler = false;	///< une sauvegarde pendant la compilation
				NkVector<NkString> sortie;	///< ce que clang a dit (la derniere fois)
				NkVector<NkErreurScriptCpp> erreurs;
				uint32 compilations = 0u;
				float32 ageReleve = 99.f;

				// --- Les Blueprints du projet ------------------------------------
				NkVector<NkString> blueprints; ///< refs (« Contenu/Scripts/Porte.nkbp »)
				NkVector<int64> empreintesBp;

				// --- Le Journal --------------------------------------------------
				/// Les lignes a recopier dans l'onglet Journal (videes par la trame).
				NkVector<NkString> journal;
				bool montrerJournal = false;

				// --- La page du graphe ouverte (NkEditeurGraphe.h) ---------------
				NkEditeurGrapheEtat graphe;

				NkEtatJeu etatPrecedent = NkEtatJeu::NK_EDITION;
				bool demarre = false;
				/// false : ne lance JAMAIS d'editeur de texte (bancs, captures : aucune
				/// fenetre ne doit s'ouvrir sur le bureau de l'utilisateur).
				bool ouvrirTexteExterne = true;

				NkEditeurScripts() = default;
				NkEditeurScripts(const NkEditeurScripts &) = delete;
				NkEditeurScripts &operator=(const NkEditeurScripts &) = delete;
				/// ⚠️ Les instances C++ sont detruites AVANT que leur DLL ne parte
				/// (les membres sont detruits dans l'ordre inverse : `modules`, qui
				/// decharge, passerait avant `hote`).
				~NkEditeurScripts() {
					hote.Arreter();
				}
		};

		/// Branche le service sur la scene du modele (l'hote, les actions), et
		/// pose `m.scripts`. A appeler une fois, apres la creation de la scene.
		void NkEditeurScriptsDemarrer(NkEditeurScripts &s, NkEditeurModele &m, const unkeny::NkActions *actions,
									  const unkeny::NkLiaisons *liaisons);
		/// Une trame : suit le projet, compile, recharge, relaie le journal, suit
		/// Jouer / Arreter. `ui` peut etre nul (bancs).
		void NkEditeurScriptsTrame(NkEditeurScripts &s, NkEditeurModele &m, NkEditeurInterface *ui, float32 dt);
		/// Avant la destruction : instances detruites, DLL dechargee.
		void NkEditeurScriptsArreter(NkEditeurScripts &s);

		// --- Le C++ ------------------------------------------------------------
		/// Releve les sources du projet ; rend true si l'une a change (ou est
		/// nouvelle, ou a disparu) depuis le releve precedent.
		bool NkEditeurScriptsReleverCpp(NkEditeurScripts &s, NkEditeurModele &m);
		/// Lance la compilation (sans attendre). false : rien a compiler, ou
		/// compilateur / en-tete introuvables (le journal le dit).
		bool NkEditeurScriptsCompiler(NkEditeurScripts &s, NkEditeurModele &m);
		/// Suit la compilation en cours ; a la fin, recharge ou rapporte.
		/// `attendre` : bloque jusqu'a la fin (bancs, ligne de commande).
		void NkEditeurScriptsSuivreCompilation(NkEditeurScripts &s, NkEditeurModele &m, bool attendre);
		/// clang++ trouve (PATH, puis msys2), ou vide.
		NkString NkEditeurScriptsCompilateur();
		/// Les classes declarees dans un texte source (NK_UNKENY_CLASSE...).
		void NkEditeurScriptsClassesDe(const char *texte, NkVector<NkString> &sortie);
		/// Le texte du registre genere pour `classes`.
		NkString NkEditeurScriptsRegistre(const NkVector<NkString> &classes);

		// --- Creer et ouvrir ---------------------------------------------------
		/// Un nouveau script C++ (modele commente) dans le dossier du navigateur
		/// `dossierNav` ; rend son chemin du navigateur (vide : echec, annonce).
		NkString NkEditeurNouveauScriptCpp(NkEditeurModele &m, const char *dossierNav);
		/// Un nouveau Blueprint (« Debut -> Afficher "Bonjour" », compile).
		NkString NkEditeurNouveauBlueprint(NkEditeurModele &m, const char *dossierNav);
		/// Ouvre un fichier dans NKCode s'il est construit a cote, sinon dans
		/// l'editeur de texte du systeme. false : rien n'a pu le lancer.
		bool NkEditeurOuvrirTexteExterne(const char *chemin);
		/// La REFERENCE d'un fichier du projet (« Contenu/Scripts/Porte.nkbp ») :
		/// relative au projet, barres obliques. Vide si hors du projet.
		NkString NkEditeurRefScript(NkEditeurModele &m, const char *cheminAbsolu);

		/// Les scripts que l'on peut poser sur une entite : les Blueprints du
		/// projet et les classes C++ du registre (« cpp:Porte »).
		void NkEditeurScriptsProposes(NkEditeurScripts &s, NkVector<NkString> &sortie);

		/// Le banc des scripts de l'editeur (Script/NkEditeurBancScripts.cpp) :
		/// lance par --selftest. 0 = tout tient (un temoin INDETERMINE n'echoue pas).
		int32 NkEditeurLancerBancScripts();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTS_H__
