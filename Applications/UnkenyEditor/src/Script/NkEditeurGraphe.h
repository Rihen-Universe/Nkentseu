//
// NkEditeurGraphe.h
// =============================================================================
// Description :
//   LA PAGE D'UN BLUEPRINT dans UnkenyEditor (document 01, § 4.7 et § 15) : un
//   double-clic sur un .nkbp du Contenu ouvre L'EDITEUR DE BLUEPRINT A LA UE5
//   (Script/NkEditeurBlueprint.h) -- un ensemble autonome -- DANS SON ONGLET DE
//   DOCUMENT (2026-10-02, Editeur/NkEditeurDocuments.h) : chaque Blueprint
//   ouvert a le sien, a cote de celui de la scene, qui reste accessible d'un
//   clic. Au premier plan, l'editeur occupe le CORPS de la fenetre (comme les
//   pages Animation / Animateur et les editeurs d'assets d'Unreal) : sa barre,
//   Mon Blueprint, la toile, ses Details, ses resultats. Ce fichier est la
//   seule colle entre lui, les scripts et la scene.
//
// Caracteristiques :
//   - Plusieurs Blueprints ouverts : NkEditeurScripts::graphes (un etat par
//     onglet, son historique et sa vue gardes) et `courant` (celui que la
//     page montre, que Compiler et Fermer visent).
//   - L'HOTE de l'editeur : compiler = ecrire le .nkbp (GRAF + MODL + DOCU) et
//     donner le module au registre ; pendant JEU, le comportement change tout
//     de suite (l'hote des scripts migre ses instances, les variables
//     restent). « Simuler » = Jouer la scene, la trace allumee. « Fermer »
//     passe par la barre (son voisin de gauche, ou la scene, passe devant).
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
		struct NkEditeurInterface;

		/// UN Blueprint ouvert (son onglet de document) : l'etat de l'editeur.
		using NkEditeurGrapheEtat = NkEditeurBlueprintEtat;

		/// Ouvre le .nkbp `chemin` (absolu) dans SON onglet, qui devient le graphe
		/// COURANT. Deja ouvert : il redevient courant (pas de second onglet).
		/// `ui` non nul : son onglet passe au premier plan (NkEditeurActiverDocument) ;
		/// nul : un banc sans interface. false : illisible (annonce).
		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin, NkEditeurInterface *ui = nullptr);
		/// Ferme le graphe COURANT, sans interface (bancs). Avec elle, passer par la
		/// barre (NkEditeurFermerDocument) : le voisin passe devant.
		void NkEditeurFermerGraphe(NkEditeurScripts &s);
		/// Le Blueprint `id`, ou nul.
		NkEditeurGrapheEtat *NkEditeurGrapheParId(NkEditeurScripts &s, nk_uint64 id);
		/// `id` devient le graphe courant. false : il n'existe pas.
		bool NkEditeurGrapheDevenirCourant(NkEditeurScripts &s, nk_uint64 id);
		/// Retire le Blueprint `id` (la barre l'a deja fait passer derriere).
		bool NkEditeurDetruireGraphe(NkEditeurScripts &s, nk_uint64 id);
		/// Compile, enregistre et donne au registre le graphe COURANT. false :
		/// l'erreur est dans `s.Graphe().message`, son noeud entoure (et sa raison
		/// ecrite dedans).
		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m);
		/// Un Blueprint est-il ouvert (un graphe courant existe) ?
		bool NkEditeurGrapheOuvert(const NkEditeurModele &m);
		/// Le rectangle du corps ou l'editeur se dessine : sous les onglets de
		/// document, jusqu'a la barre d'etat, toute la largeur.
		nkgui::NkRect NkEditeurZoneGraphe(const NkEditeurInterface &ui);
		/// Dessine le graphe courant dans le CORPS -- quand son onglet est au
		/// premier plan (NkEditeurBlueprintDevant). false : aucun a dessiner (la
		/// scene se dessine).
		bool NkEditeurDessinerGraphe(NkEditeurCadre &c);
		/// Pose un noeud du catalogue au centre de ce qu'on regarde.
		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type);
		/// L'hote de l'editeur de Blueprint, branche sur les scripts et la scene
		/// (`ui` : la barre des documents, pour « Fermer » ; nul dans un banc).
		NkHoteBlueprint NkEditeurHoteBlueprint(NkEditeurScripts &s, NkEditeurModele &m, NkEditeurInterface *ui = nullptr);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__
