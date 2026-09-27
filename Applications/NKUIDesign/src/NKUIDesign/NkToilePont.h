#pragma once
// -----------------------------------------------------------------------------
// @File    NkToilePont.h
// @Brief   Le pont entre la vue de NKUIDesign (`NkCanvasView`) et celle du rôle
//          `Canvas` (`NkGuiVue`) — deux ecritures d'UNE SEULE transformation.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE AVANT LE CHANTIER QU'IL SERT
// =============================================================================
//  Pour faire entrer la toile de NKUIDesign dans le role `Canvas`, il faut
//  d'abord repondre a une question : les deux vues decrivent-elles LA MEME
//  transformation, ou deux transformations differentes ?
//
//     NkCanvasView   ecran = origine_du_viewport + pan + doc x zoom
//     NkGuiVue       ecran = centre_de_la_zone   + (monde - centre) x zoom
//
//  Si c'est la meme, le passage de l'une a l'autre est un simple changement de
//  parametrage et l'integration est sure. Si ce n'est pas la meme, le plan de
//  reprise est faux et il vaut mieux le savoir AVANT d'ouvrir `Panels.h`.
//
//  🔴 CE FICHIER NE TOUCHE A RIEN DE VIVANT. Il n'est appele par aucun chemin de
//     l'application : il existe pour etre MESURE (`--sonde-pont`). Ecrire la
//     conversion et la prouver coute une heure ; decouvrir qu'elle etait fausse
//     apres avoir demonte l'hebergement de la toile en coute beaucoup plus.
//
// =============================================================================
//  LA CONVERSION, ET D'OU ELLE SORT
// =============================================================================
//  Le point document qui tombe au CENTRE de la zone, lu par la vue de
//  NKUIDesign :
//
//      docCentre.x = ToDocX(viewport.x + w/2) = (w/2 - panX) / zoom
//
//  C'est exactement le `centre` de `NkGuiVue`. Et l'inverse :
//
//      panX = w/2 - centre.x * zoom
//
//  ⚠️ LE `zoom` EST LE MEME DES DEUX COTES, et ce n'est pas une chance : les deux
//     comptent des PIXELS ECRAN PAR UNITE. Si l'un avait compte l'inverse, la
//     conversion aurait demande une division et la moindre erreur de sens se
//     serait vue seulement a fort zoom.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_TOILEPONT_H__
#define __NKENTSEU_NKUIDESIGN_TOILEPONT_H__

#include "NKGui/Doc/NkGuiMonteur.h" // NkGuiVue
#include "Canvas.h"					// NkCanvasView

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::nkgui::NkGuiVue;
	using nkentseu::nkgui::NkRect;

	/// La vue de NKUIDesign, ecrite comme celle du role `Canvas`.
	inline NkGuiVue NkVueDepuisToile(const NkCanvasView &v) noexcept {
		NkGuiVue g;
		const float32 z = v.zoom > 0.0001f ? v.zoom : 1.f;
		g.zoom = z;
		g.centre = nkentseu::nkgui::NkVec2((v.viewport.w * 0.5f - v.panX) / z,
										   (v.viewport.h * 0.5f - v.panY) / z);
		return g;
	}

	/// L'inverse, dans la zone `r`.
	///
	/// ⚠️ ELLE POSE AUSSI LE `viewport`, et c'est indispensable : sans lui,
	///    `ToScreen` ajouterait l'origine d'une zone perimee. Le rectangle fait
	///    partie de la vue chez NKUIDesign, il n'en fait pas partie chez le role --
	///    c'est la seule asymetrie des deux ecritures, et elle se paie ici.
	inline NkCanvasView NkToileDepuisVue(const NkGuiVue &g, const NkRect &r) noexcept {
		NkCanvasView v;
		const float32 z = g.zoom > 0.0001f ? g.zoom : 1.f;
		v.zoom = z;
		v.viewport = {r.x, r.y, r.w, r.h};
		v.panX = r.w * 0.5f - g.centre.x * z;
		v.panY = r.h * 0.5f - g.centre.y * z;
		return v;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_TOILEPONT_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
