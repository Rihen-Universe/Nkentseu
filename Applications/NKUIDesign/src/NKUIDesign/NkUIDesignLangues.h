#pragma once
// -----------------------------------------------------------------------------
// @File    NkUIDesignLangues.h
// @Brief   La table de traduction de NKUIDesign — la premiere application a
//          recevoir le mecanisme descendu dans NKGui.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CETTE TABLE EST ICI ET NON DANS NKGui
// =============================================================================
//  Ce sont les libelles de NKUIDesign. NKCode a les siens (1 541 entrees),
//  NkAnima aura les siens. Les coudre ensemble dans le noyau donnerait un NKGui
//  qui grossit a chaque application, et des cles qui finiraient par se marcher
//  dessus.
//
//  Chaque application POSE donc sa table, comme elle pose deja ses actions, ses
//  zones, ses jetons, ses icones et ses ecouteurs. Sixieme table de la meme
//  forme : rien de nouveau a apprendre.
//
// =============================================================================
//  L'ETAT DES COLONNES, DIT FRANCHEMENT
// =============================================================================
//  Colonnes, dans l'ordre de `NkGuiCodeLangue` :
//      0 fr   1 en   2 es   3 pt   4 de   5 it   6 ru   7 gom
//
//  ⚠️ SEULS LE FRANCAIS ET L'ANGLAIS SONT REMPLIS. Les six autres colonnes sont
//     VIDES, et c'est un etat assume, pas un oubli : une colonne vide retombe sur
//     l'anglais (`NkGuiTexteLangue`, regle 3), donc l'interface reste UTILISABLE
//     dans les huit langues pendant qu'on traduit.
//
//     La tentation etait de remplir les six autres a la main. Je ne l'ai pas
//     fait : une traduction inventee par quelqu'un qui ne parle pas la langue est
//     pire qu'une absence -- elle a l'air juste, donc personne ne la corrige. Le
//     Ghɔmáláʼ de NKCode a ete ecrit par quelqu'un qui le parle ; celui-ci
//     attendra la meme chose.
//
//  📌 ET LE COMPTEUR LE DIT : `NkGuiLanguesReleve().repliAnglais` monte des qu'on
//     bascule dans une langue non remplie. C'est ce chiffre qui mesurera le
//     travail de traduction restant, au lieu d'une impression.
//
// =============================================================================
//  ⚠️ LES CLES SONT LES IDENTIFIANTS D'ACTION, ET CE N'EST PAS UN HASARD
// =============================================================================
//  `design.enregistrer` est a la fois l'identifiant du widget, le nom de son
//  action, et la cle de son libelle. Une seule chaine a retenir, et une seule a
//  ecrire dans le document -- au lieu d'un identifiant, d'un nom d'action et
//  d'une cle de traduction qui divergeraient au premier renommage.
//
//  Exception assumee : quelques libelles ne sont pas des actions (le titre d'un
//  menu, un en-tete). Ils portent alors une cle en `design.ui.*`.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_LANGUES_H__
#define __NKENTSEU_NKUIDESIGN_LANGUES_H__

#include "NKGui/Doc/NkGuiLangues.h"

namespace nkuidesign {

	/// La table. `static` : elle doit survivre a l'interface (voir
	/// `NkGuiPoserTableLangues` — la table n'est PAS copiee).
	inline const nkentseu::nkgui::NkGuiTraduction *NkUIDesignTableLangues(nkentseu::int32 &n) {
		using nkentseu::nkgui::NkGuiTraduction;
		static const NkGuiTraduction T[] = {
			// ── Le menu Design ────────────────────────────────────────────
			{"design.ui.menuDesign", {"Design", "Design", "", "", "", "", "", ""}},
			{"design.enregistrer", {"Enregistrer le document", "Save document", "", "", "", "", "", ""}},
			{"design.recharger", {"Recharger depuis le disque", "Reload from disk", "", "", "", "", "", ""}},
			{"design.nouveau", {"Nouveau document", "New document", "", "", "", "", "", ""}},
			{"design.annuler", {"Annuler", "Undo", "", "", "", "", "", ""}},
			{"design.retablir", {"Retablir", "Redo", "", "", "", "", "", ""}},
			{"design.grouper", {"Grouper", "Group", "", "", "", "", "", ""}},
			{"design.grouperSelection", {"Grouper la selection", "Group selection", "", "", "", "", "", ""}},
			{"design.degrouper", {"Degrouper", "Ungroup", "", "", "", "", "", ""}},
			{"design.dupliquer", {"Dupliquer", "Duplicate", "", "", "", "", "", ""}},
			{"design.exporter", {"Exporter", "Export", "", "", "", "", "", ""}},
			{"design.exporterPoints", {"Exporter...", "Export...", "", "", "", "", "", ""}},
			// ── Le menu Vues ──────────────────────────────────────────────
			{"design.ui.menuVues", {"Vues", "Views", "", "", "", "", "", ""}},
			{"design.vue.hierarchie", {"Hierarchie", "Hierarchy", "", "", "", "", "", ""}},
			{"design.vue.inspecteur", {"Inspecteur", "Inspector", "", "", "", "", "", ""}},
			{"design.vue.hierarchieLong", {"Vue : hierarchie", "View: hierarchy", "", "", "", "", "", ""}},
			{"design.vue.inspecteurLong", {"Vue : inspecteur", "View: inspector", "", "", "", "", "", ""}},
			{"design.vue.palette", {"Palette", "Palette", "", "", "", "", "", ""}},
			{"design.vue.composition", {"Composition", "Composition", "", "", "", "", "", ""}},
			{"design.vue.proprietes", {"Proprietes", "Properties", "", "", "", "", "", ""}},
			{"design.vue.preferences", {"Preferences", "Preferences", "", "", "", "", "", ""}},
			// ── La toile ──────────────────────────────────────────────────
			{"design.ui.toile", {"Toile", "Canvas", "", "", "", "", "", ""}},
		};
		n = (nkentseu::int32)(sizeof(T) / sizeof(T[0]));
		return T;
	}

	/// Pose la table. A appeler tot — avant tout montage, sondes comprises.
	///
	/// ⚠️ AVANT TOUT MONTAGE, ET PAS SEULEMENT AVANT LA FENETRE. Une sonde qui
	///    monte un document sans table posee verrait tous ses libelles retomber
	///    sur leur cle -- et son releve dirait « des libelles bizarres » la ou il
	///    n'y a qu'un ordre d'appel.
	///
	/// 🔴 ELLE N'A PAS DE GARDE « DEJA POSEE », ET ELLE EN A EU UN. Premiere
	///    version : un `static bool posee` qui sortait au deuxieme appel. Defaut
	///    attrape par le critere C7 de `--sonde-langues` : 14 cles demandees,
	///    ZERO servie. La sonde appelle `NkGuiOublierTablesLangues()` pour repartir
	///    propre, puis repose -- et le garde refusait, laissant le registre VIDE.
	///
	///    Une fonction ne peut pas se declarer idempotente sur la foi d'un drapeau
	///    LOCAL quand l'etat qu'elle pose vit AILLEURS et qu'un tiers peut le
	///    vider. Le drapeau disait « c'est fait » ; ce n'etait plus vrai.
	///
	///    Reposer deux fois est sans danger : la recherche prend la PREMIERE table
	///    qui repond, donc un doublon rend la meme chose.
	inline void NkUIDesignPoserLangues() noexcept {
		nkentseu::int32 n = 0;
		const nkentseu::nkgui::NkGuiTraduction *T = NkUIDesignTableLangues(n);
		(void)nkentseu::nkgui::NkGuiPoserTableLangues(T, n);
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_LANGUES_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
