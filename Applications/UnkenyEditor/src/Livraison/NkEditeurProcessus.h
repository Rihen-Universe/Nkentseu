//
// NkEditeurProcessus.h
// =============================================================================
// Description :
//   Lancer un programme externe (jenga) SANS geler l'editeur, lire sa sortie
//   ligne par ligne pendant qu'il tourne, et pouvoir l'ARRETER.
//
// Caracteristiques :
//   - Un fil d'arriere-plan lit la sortie (stdout ET stderr fusionnes) et la
//     range dans une file protegee ; l'interface la RECOLTE a chaque trame.
//   - Windows : CreateProcessW + tuyau, CREATE_NO_WINDOW (l'editeur est une
//     application fenetree : un _popen y ouvrirait une console noire a chaque
//     construction), et TerminateProcess pour « Annuler ».
//   - Ailleurs (Linux, macOS) : popen ; « Annuler » n'y est pas tenu, et
//     EnCours() le dit en restant vrai jusqu'a la fin.
//   - Les sequences ANSI (couleurs de jenga) sont retirees : le Journal de
//     l'editeur n'est pas un terminal.
//
// ⚠️ UN LANCEUR DE PLUS, ET C'EST SU
//   Le depot en a deja : NKCode (Project/NkProcess.h, _popen sans arret),
//   NkLsp.cpp (CreateProcessW), NKConverse, NKCraft (Genia). Le document 01
//   d'UnkenyEditor (§ 5.3, risque R9) demande leur EXTRACTION en Kernel/System
//   (NkProcessus) au palier S5. Celui-ci est ecrit pour pouvoir y descendre
//   tel quel : il ne depend que de NKThreading et NKContainers.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURPROCESSUS_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURPROCESSUS_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThread.h"

namespace nkentseu {
	namespace editeur {

		class NkEditeurProcessus {
			public:
				NkEditeurProcessus() = default;
				~NkEditeurProcessus();

				NkEditeurProcessus(const NkEditeurProcessus &) = delete;
				NkEditeurProcessus &operator=(const NkEditeurProcessus &) = delete;

				/// Une variable d'environnement pour le programme lance (et pour
				/// l'editeur lui-meme : le fils herite de l'environnement du pere).
				/// A poser avant Lancer.
				void Environnement(const char *nom, const char *valeur);

				/// Lance `commande` (une ligne de commande complete, programme
				/// cherche dans le PATH) dans le dossier `dossier` (vide = celui de
				/// l'editeur). false si un processus tourne deja, ou si le systeme a
				/// refuse -- la raison est alors la premiere ligne recoltee.
				bool Lancer(const NkString &commande, const NkString &dossier);

				/// Les lignes arrivees depuis le dernier appel (la file est videe).
				void Recolter(NkVector<NkString> &lignes);

				bool EnCours() const;
				/// Le code de sortie du programme, valable quand EnCours() est faux
				/// apres un Lancer reussi. -1 : il n'a pas pu etre lance ; 124 : il
				/// a ete arrete.
				int32 Code() const;
				/// Arrete le programme (Windows). Sans effet ailleurs.
				void Arreter();
				/// Attend la fin en transmettant chaque ligne a `sortie` au fil de
				/// l'eau -- la ligne de commande de l'editeur (`--construire=`).
				int32 Attendre(void (*sortie)(const NkString &ligne, void *donnees), void *donnees);

			private:
				void Lire();
				void Pousser(const NkString &ligne);
				void Terminer(int32 code);

				threading::NkThread mFil;
				mutable threading::NkMutex mVerrou;
				NkVector<NkString> mLignes; ///< protege par mVerrou
				NkString mCommande;
				NkString mDossier;
				NkVector<NkString> mNomsEnv;
				NkVector<NkString> mValeursEnv;
				bool mEnCours = false;		///< protege par mVerrou
				int32 mCode = 0;			///< protege par mVerrou
				void *mProcessus = nullptr; ///< HANDLE du processus (Windows)
				bool mArrete = false;
		};

		/// Retire les sequences d'echappement ANSI (ESC [ ... lettre) et les
		/// fins de ligne d'une ligne de sortie.
		NkString NkSansAnsi(const char *ligne);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURPROCESSUS_H__
