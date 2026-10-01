//
// NkEditeurGraphe.h
// =============================================================================
// Description :
//   LA PAGE D'UN BLUEPRINT dans UnkenyEditor (document 01, § 4.7) : un
//   double-clic sur un .nkbp du Contenu ouvre L'EDITEUR DE BLUEPRINT A LA UE5
//   (Script/NkEditeurBlueprint.h) -- un ensemble autonome -- EN PLEIN, a la
//   place de la vue : Placer des acteurs, l'Outliner, les Details de la scene
//   et le tiroir se replient tant qu'il est ouvert, et reviennent a la
//   fermeture. (L'integration le mettra dans un onglet de document : ce
//   fichier est la seule colle.)
//
// Caracteristiques :
//   - L'HOTE de l'editeur : compiler = ecrire le .nkbp (GRAF + MODL + DOCU) et
//     donner le module au registre ; pendant JEU, le comportement change tout
//     de suite (l'hote des scripts migre ses instances, les variables
//     restent). « Simuler » = Jouer la scene, la trace allumee.
//   - Les noms d'avant le 01/10 (NkEditeurOuvrirGraphe...) restent : le
//     navigateur, les Details de la scene et les bancs les appellent.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__

#include "NKContainers/String/NkString.h"
#include "Script/NkEditeurBlueprint.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurScripts;
		struct NkEditeurModele;
		struct NkEditeurCadre;

		/// L'etat de la page : celui de l'editeur de Blueprint.
		using NkEditeurGrapheEtat = NkEditeurBlueprintEtat;

		/// Ouvre le .nkbp `chemin` (absolu). false : illisible (annonce).
		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin);
		void NkEditeurFermerGraphe(NkEditeurScripts &s);
		/// Compile, enregistre et donne au registre. false : l'erreur est dans
		/// `s.graphe.message`, son noeud entoure (et sa raison ecrite dedans).
		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m);
		/// La page est-elle ouverte ? (le viseur lui cede sa place)
		bool NkEditeurGrapheOuvert(const NkEditeurModele &m);
		/// Dessine la page (en plein : la colonne centrale, panneaux replies).
		void NkEditeurDessinerGraphe(NkEditeurCadre &c);
		/// Pose un noeud du catalogue au centre de ce qu'on regarde.
		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type);
		/// L'hote de l'editeur de Blueprint, branche sur les scripts et la scene.
		NkHoteBlueprint NkEditeurHoteBlueprint(NkEditeurScripts &s, NkEditeurModele &m);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__
