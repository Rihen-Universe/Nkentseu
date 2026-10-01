//
// NkEditeurDeroulement.h
// =============================================================================
// Description :
//   Le DEROULEMENT d'une construction preparee (NkPreparerConstruction) : les
//   commandes Jenga l'une apres l'autre, le verrou et le sceau du moteur
//   precompile, le rangement, la verification -- et le JOURNAL qui en sort,
//   range par phases, niveaux et diagnostics.
//
// Caracteristiques :
//   - UN SEUL deroulement pour la fenetre « Construire » (une trame, un
//     Avancer) et pour `UnkenyEditor --construire=` (une boucle) : la ligne de
//     commande mesure exactement ce que la fenetre fait.
//   - NkJournalConstruction lit la sortie de Jenga telle quelle (couleurs deja
//     retirees par NkEditeurProcessus) : il en ote les cadres (╔═║…), RECOUD
//     les lignes que Jenga a coupees a 92 colonnes dans ses boites d'erreur,
//     classe chaque ligne (erreur, avertissement, information, note) et en
//     tire les diagnostics « fichier:ligne:colonne: message » et la
//     progression (projets, fichiers compiles / total). `construire.log`
//     garde, lui, le texte BRUT.
//
// ⚠️ LA PROGRESSION ET LES DIAGNOSTICS SONT LUS DANS L'AFFICHAGE DE JENGA
//   Jenga a une sortie structuree (Utils/Reporter.SetBuildSink) mais seulement
//   pour qui l'embarque en Python ; un processus separe n'a que la console.
//   Les motifs lus ici (« Build Order (N projects) », « [i/N] Compiled »,
//   « Linking... », la largeur 92 des boites) sont ceux de Jenga 2.8.7
//   (Utils/Reporter.py, BuildLogger) : s'ils changent, la barre se fige et les
//   diagnostics se raréfient -- la construction, elle, ne change pas, et le
//   banc (lg11, lg12) le dit.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURDEROULEMENT_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURDEROULEMENT_H__

#include "Livraison/NkEditeurConstruire.h"
#include "Livraison/NkEditeurMoteur.h"
#include "Livraison/NkEditeurProcessus.h"

namespace nkentseu {
	namespace editeur {

		enum class NkNiveauLigne : uint8 {
			NK_INFO = 0,
			NK_SUCCES,
			NK_AVERTISSEMENT,
			NK_ERREUR,
			NK_NOTE,  ///< extrait de source, « In file included from », banniere
			NK_ETAPE, ///< ce que l'editeur annonce d'une phase ou d'une commande
		};

		struct NkLigneJournal {
				NkNiveauLigne niveau = NkNiveauLigne::NK_INFO;
				NkPhaseConstruction phase = NkPhaseConstruction::NK_PREPARER;
				float32 temps = 0.f; ///< secondes depuis le debut de la construction
				NkString texte;
		};

		/// Une erreur ou un avertissement du compilateur (ou de l'editeur de
		/// liens : `fichier` vide, `ligne` 0).
		struct NkDiagnostic {
				NkString fichier;
				int32 ligne = 0;
				int32 colonne = 0;
				NkString message;
				bool erreur = true;
		};

		class NkJournalConstruction {
			public:
				void Vider();
				void Phase(NkPhaseConstruction p) noexcept {
					mPhase = p;
				}
				/// Une ligne de l'editeur lui-meme.
				void Annonce(const NkString &texte, NkNiveauLigne niveau, float32 temps);
				/// Une ligne de Jenga, sans couleurs mais avec ses cadres.
				void LigneJenga(const NkString &brute, float32 temps);
				/// Fin d'une commande : la ligne recousue en cours part au journal.
				void FinCommande(float32 temps);
				/// Remet la progression a zero (chaque commande a la sienne).
				void NouvelleCommande();
				/// La part faite de la commande en cours, 0..1 ; -1 si Jenga n'a
				/// encore rien dit de ce qu'il y a a faire.
				float32 Fraction() const noexcept;
				/// Le niveau d'une ligne lue hors des boites de Jenga.
				static NkNiveauLigne NiveauDe(const NkString &texte);

				NkVector<NkLigneJournal> lignes;
				NkVector<NkDiagnostic> diagnostics;
				int32 erreurs = 0;
				int32 avertissements = 0;
				// La progression de la commande en cours.
				int32 projetsTotal = 0;
				int32 projetsFaits = 0;
				int32 fichiersTotal = 0;
				int32 fichiersFaits = 0;
				NkString projet;  ///< le projet que Jenga construit
				bool lien = false; ///< « Linking... » vu pour ce projet

			private:
				void Pousser(NkNiveauLigne niveau, const NkString &texte, float32 temps);
				void Logique(const NkString &texte, float32 temps); ///< une ligne recousue d'une boite
				void Diagnostiquer(const NkString &texte);
				void Progression(const NkString &texte);

				NkPhaseConstruction mPhase = NkPhaseConstruction::NK_PREPARER;
				/// 0 hors boite ; 1 en-tete attendu ; 2 erreur ; 3 avertissement ; 4 autre.
				int32 mBoite = 0;
				NkString mRecousue;
				int32 mLargeur = 0; ///< colonnes du dernier morceau de boite
		};

		class NkDeroulementConstruction {
			public:
				/// Apres NkPreparerConstruction reussie : `preparation` (ses lignes)
				/// va au journal, puis la premiere commande part. `plan` doit
				/// vivre aussi longtemps que le deroulement.
				void Commencer(const NkDemandeConstruction &demande, NkPlanConstruction &plan, const NkVector<NkString> &preparation);
				/// Une trame (ou un tour de la ligne de commande). false = fini.
				bool Avancer();
				/// « Arreter » : la commande en cours est tuee, la construction
				/// echoue (code 124), le verrou est rendu.
				void Arreter();

				bool EnCours() const noexcept {
					return mPas != NkPas::NK_FINI && mPas != NkPas::NK_REPOS;
				}
				bool Verification() const noexcept {
					return mPas == NkPas::NK_VERIFICATION;
				}
				bool Reussi() const noexcept {
					return mReussi;
				}
				/// La progression de toute la construction, 0..1.
				float32 Progression() const;
				/// Les secondes depuis Commencer (figees a la fin).
				float32 Temps() const;

				NkJournalConstruction journal;
				NkString annonce; ///< la ligne d'etat
				NkPlanConstruction *plan = nullptr;

			private:
				enum class NkPas : uint8 { NK_REPOS = 0, NK_LANCER, NK_VERROU, NK_PROCESSUS, NK_VERIFICATION, NK_FINI };

				void Lancer();
				void FinCommande(int32 code);
				void Achever();
				void Echec(const NkString &pourquoi);
				void Annoncer(const NkString &texte, NkNiveauLigne niveau);
				void Ouvrir(NkPhaseConstruction p);
				void Fermer(NkPhaseConstruction p, NkEtatPhase etat, const NkString &detail);
				void Recolter();

				NkDemandeConstruction mDemande;
				NkEditeurProcessus mProcessus;
				NkVerrouMoteur mVerrou;
				NkPas mPas = NkPas::NK_REPOS;
				usize mEtape = 0;
				bool mReussi = false;
				bool mAttenteDite = false;
				float64 mDebut = 0.0;
				float64 mFin = 0.0;
				float64 mDebutMoteur = 0.0;
		};

		/// Le resume des erreurs, en tete de l'onglet Journal : « fichier:ligne:
		/// message », le fichier par son NOM seul. Au plus `max` lignes.
		NkVector<NkString> NkResumeErreurs(const NkJournalConstruction &j, usize max);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURDEROULEMENT_H__
