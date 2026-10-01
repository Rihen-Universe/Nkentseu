//
// NkEditeurExemplePortes.h
// =============================================================================
// Description :
//   LE PROJET D'EXEMPLE des scripts, pret a essayer : « Portes ». Un Joueur
//   (controleur de personnage : fleches / A-D pour marcher, Espace pour sauter)
//   entre a gauche dans une zone dont le BLUEPRINT ouvre la porte bleue, a
//   droite dans une zone dont le script C++ ouvre la porte rouge. Les deux
//   portes montent de 2 m et lancent leurs etincelles (l'effet, R34).
//
// Caracteristiques :
//   - Ecrit un VRAI projet : Contenu/Scenes/Portes.nkscene,
//     Contenu/Scripts/PorteBlueprint.nkbp (graphe construit par code et
//     compile), Contenu/Scripts/PorteCpp.cpp (source commentee). L'editeur le
//     compile au chargement et le recompile a chaque enregistrement.
//   - `UnkenyEditor --exemple=portes` l'ecrit (s'il n'existe pas) dans
//     Documents/Unkeny/Exemples/Portes/ et l'ouvre ; `--exemple=portes-neuf`
//     le reecrit.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEUREXEMPLEPORTES_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEUREXEMPLEPORTES_H__

#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace unkeny {
		class NkScene;
	}
	namespace editeur {

		/// La source du script C++ de la porte rouge (aussi lue par le banc).
		const char *NkEditeurSourcePorteCpp() noexcept;

		/// Construit la scene des portes dans `scene` (re-initialisee).
		void NkEditeurScenePortes(unkeny::NkScene &scene);

		/// Ecrit le projet dans `dossier` (barre finale). Rend le chemin de la
		/// scene, vide si une ecriture a echoue (`erreur` dit laquelle).
		NkString NkEditeurEcrireExemplePortes(const char *dossier, NkString *erreur = nullptr);

		/// Documents/Unkeny/Exemples/Portes/ (barre finale).
		NkString NkEditeurDossierExemplePortes();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEUREXEMPLEPORTES_H__
