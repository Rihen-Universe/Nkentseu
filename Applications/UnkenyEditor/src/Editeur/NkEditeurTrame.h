//
// NkEditeurTrame.h
// =============================================================================
// Description :
//   LA TRAME de l'editeur, sans fenetre : l'ordre de dessin des panneaux, le
//   masquage des gestes sous un menu ou une boite, les menus avec l'entree
//   reelle, puis les raccourcis. NkEditeurApp::OnDraw l'appelle entre ce qui
//   est a la FENETRE (plan, curseur, liseret, fin de trame de la souris).
//
// Caracteristiques :
//   - (2026-09-30) Sortie de NkEditeurApp::OnDraw pour que le banc la joue
//     TELLE QUELLE dans un vrai NkGuiContext : le clic droit de l'Outliner et
//     du navigateur « ne faisait rien » a l'ecran alors que chaque panneau,
//     eprouve seul, ouvrait son menu. Un defaut d'ORDRE ne se voit qu'avec
//     tous les panneaux dans l'ordre de la vraie trame (temoin e48).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURTRAME_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURTRAME_H__

#include "Editeur/NkEditeurEntrees.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurSelecteur.h"
#include "Livraison/NkEditeurFenetreConstruire.h"

namespace nkentseu {
	namespace editeur {

		/// Le corps de la trame : bords, panneaux (gestes neutralises sous un
		/// menu), menus, boite modale, raccourcis, journal. Rien de la fenetre.
		void NkEditeurDessinerTrame(NkEditeurCadre &c, NkEditeurEntrees &entrees, NkEditeurConstruction &construction);
		/// La meme, avec LE selecteur de fichiers (NkEditeurSelecteur.h) : les
		/// demandes Importer / Exporter l'ouvrent, et il est modal. Nul : pas de
		/// selecteur (les demandes sont oubliees).
		void NkEditeurDessinerTrame(NkEditeurCadre &c, NkEditeurEntrees &entrees, NkEditeurConstruction &construction,
									NkEditeurSelecteurEtat *selecteur);

		/// Les raccourcis globaux, APRES le dessin : un champ de saisie focalise
		/// les a vus passer et les garde pour lui.
		void NkEditeurRaccourcis(NkEditeurCadre &c);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURTRAME_H__
