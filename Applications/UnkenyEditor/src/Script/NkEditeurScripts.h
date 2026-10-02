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
//   - (01/10 soir) LE WORKSPACE JENGA DU PROJET, comme la solution qu'Unreal
//     genere : `<projet>/<Projet>.jenga` (NkEditeurWorkspaceCpp.h), ecrit ou
//     mis a jour des que le projet a un .cpp, sans toucher a ce que
//     l'utilisateur y a ecrit. Un double-clic sur un script C++ ouvre NKCode
//     SUR lui et sur le fichier ; « Construire » dans NKCode (jenga build)
//     produit `Intermediaire/Scripts/Jenga/Scripts.dll`, que l'editeur
//     RECHARGE A CHAUD des qu'elle a change et ne bouge plus (deux releves).
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

				// --- Le workspace Jenga des scripts (NkEditeurWorkspaceCpp.h) -----
				/// `<projet>/<Projet>.jenga`, pose quand le projet a des sources.
				NkString workspace;
				bool workspaceAssure = false; ///< deja ecrit / verifie pour ce projet
				/// La DLL que Jenga produit (Construire dans NKCode) : son empreinte
				/// au dernier chargement (ou a l'ouverture du projet : une DLL d'une
				/// session precedente n'est pas chargee), et au dernier releve -- elle
				/// n'est rechargee qu'une fois STABLE d'un releve a l'autre (l'editeur
				/// de liens a fini d'ecrire).
				int64 empreinteJenga = 0;
				int64 empreinteJengaVue = 0;
				uint32 rechargesJenga = 0u;

				// --- Les Blueprints du projet ------------------------------------
				NkVector<NkString> blueprints; ///< refs (« Contenu/Scripts/Porte.nkbp »)
				NkVector<int64> empreintesBp;

				// --- Le Journal --------------------------------------------------
				/// Les lignes a recopier dans l'onglet Journal (videes par la trame).
				NkVector<NkString> journal;
				bool montrerJournal = false;

				// --- Les Blueprints ouverts (NkEditeurGraphe.h) : UN ONGLET CHACUN --
				// (2026-10-02) Il n'y en avait qu'un, qui remplacait la vue sans
				// retour possible ; chacun a desormais son onglet de document.
				NkVector<NkEditeurGrapheEtat *> graphes;
				/// Celui que la page montre et que Compiler / Fermer visent (nul : aucun).
				NkEditeurGrapheEtat *courant = nullptr;
				nk_uint64 prochainGraphe = 1;
				/// Rendu par Graphe() quand aucun n'est ouvert (`ouvert` faux) : jamais nul.
				NkEditeurGrapheEtat aucun;
				NkEditeurGrapheEtat &Graphe() noexcept {
					return courant != nullptr ? *courant : aucun;
				}

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
					for (uint32 i = 0; i < graphes.Size(); ++i) {
						memory::NkGetDefaultAllocator().Delete(graphes[i]);
					}
					graphes.Clear();
					courant = nullptr;
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
		/// Ouvre un fichier dans l'editeur de texte du SYSTEME (l'application
		/// associee, sinon le bloc-notes). false : rien n'a pu le lancer.
		bool NkEditeurOuvrirTexteExterne(const char *chemin);

		/// Ce qu'il faut pour lancer NKCode sur un script : l'executable, ses
		/// arguments (« "<workspace .jenga>" "<script>" ») et son dossier de travail.
		struct NkLancementNKCode {
				NkString exe;	   ///< vide : NKCode introuvable
				NkString arguments;
				NkString dossier;  ///< le dossier de travail donne a NKCode
				NkString workspace;
		};
		/// NKCode : `NK_NKCODE` s'il est pose, sinon construit dans le depot
		/// (Build/Bin/{Release,Debug}-<Systeme>/NKCode/), sinon a cote de l'editeur
		/// (../NKCode/). Vide si introuvable.
		NkString NkEditeurTrouverNKCode();
		/// Le lancement de NKCode sur `script`, ouvert SUR `workspace`.
		NkLancementNKCode NkEditeurLancementNKCode(const char *workspace, const char *script);
		/// Un SCRIPT C++ du projet (double-clic, Details, « + Ajouter ») : le
		/// workspace Jenga du projet est assure, puis NKCode s'ouvre DESSUS et sur
		/// ce fichier. Sans NKCode, le Journal dit pourquoi et quoi faire, et le
		/// fichier s'ouvre dans l'editeur de texte du systeme. Sans effet quand
		/// `s.ouvrirTexteExterne` est faux (bancs). false : rien n'a pu s'ouvrir.
		bool NkEditeurOuvrirScriptCpp(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin);
		/// Assure le workspace du projet suivi (une fois par projet, ou de nouveau
		/// si `forcer`) et rend son chemin ; le Journal dit ce qui a ete fait.
		NkString NkEditeurScriptsAssurerWorkspace(NkEditeurScripts &s, bool forcer);
		/// Suit la DLL que Jenga produit : rechargee a chaud quand elle a change
		/// et ne bouge plus d'un releve a l'autre. true : un rechargement a eu lieu.
		bool NkEditeurScriptsSuivreJenga(NkEditeurScripts &s);
		/// Ecrit le registre des classes (`Intermediaire/Scripts/NkUnkRegistre.cpp`)
		/// d'apres les sources, s'il a change. Le workspace Jenga le compile aussi.
		void NkEditeurScriptsEcrireRegistre(NkEditeurScripts &s);

		/// `--preuve-nkcode=DOSSIER` : la preuve DE BOUT EN BOUT, hors ecran --
		/// l'exemple Portes, le double-clic sur PorteCpp.cpp (NKCode s'ouvre, en
		/// sonde hors ecran, sur le workspace et construit par Jenga), le
		/// rechargement a chaud de la DLL de Jenga, puis Jouer. Images et journal
		/// dans DOSSIER. 0 = tout tient.
		int32 NkEditeurPreuveNKCode(const char *dossier);
		/// La REFERENCE d'un fichier du projet (« Contenu/Scripts/Porte.nkbp ») :
		/// relative au projet, barres obliques. Vide si hors du projet.
		NkString NkEditeurRefScript(NkEditeurModele &m, const char *cheminAbsolu);

		/// Les scripts que l'on peut poser sur une entite : les Blueprints du
		/// projet et les classes C++ du registre (« cpp:Porte »).
		void NkEditeurScriptsProposes(NkEditeurScripts &s, NkVector<NkString> &sortie);

		/// Le banc des scripts de l'editeur (Script/NkEditeurBancScripts.cpp) :
		/// lance par --selftest. 0 = tout tient (un temoin INDETERMINE n'echoue pas).
		int32 NkEditeurLancerBancScripts();
		/// Le banc de l'EDITEUR DE BLUEPRINT a la UE5 (Script/NkEditeurBancBlueprint.cpp),
		/// lance a la suite du precedent.
		int32 NkEditeurLancerBancBlueprint();
		/// `--captures-scripts=DOSSIER` : les captures HORS ECRAN des scripts
		/// (page du graphe, erreur sur son noeud, Details, jeu, Journal, menu).
		int32 NkEditeurCapturesScripts(const char *dossier);
		/// `--captures-blueprint=DOSSIER` : les captures HORS ECRAN de l'editeur de
		/// Blueprint a la UE5 (vue d'ensemble, menus, variable, fonction, erreur,
		/// Simuler, noeuds de code, variables par instance).
		int32 NkEditeurCapturesBlueprint(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTS_H__
