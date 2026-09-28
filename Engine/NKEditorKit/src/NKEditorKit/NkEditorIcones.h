#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorIcones.h
// @Brief   LE JEU D'ICÔNES DES VERBES D'ÉDITEUR — tracé, partagé, sans atlas.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE
// =============================================================================
//  Rodolf, 28/09 : « les boutons annuler, rétablir, grouper, découper,
//  dupliquer, enregistrer le document, exporter — qui doivent être liés à des
//  icônes — en plus sont très mal placés ».
//
//  🔴 ET LE DÉPÔT N'AVAIT AUCUN JEU D'ICÔNES POSÉ. `NkGuiPoserIcones` existait,
//     le monteur savait peindre une clé `icon = "…"` depuis le jeu posé
//     (`NkGuiMonteur.h`), et `NkGuiIconesPosees()` rendait `nullptr` dans les
//     cinq applications : chaque `icon` écrite dans un document se serait
//     comptée `iconesManquantes` et n'aurait rien dessiné. Le mécanisme entier
//     attendait ses dessins.
//
// =============================================================================
//  ⚠️ IL VIT DANS LE KIT, PAS DANS NKUIDesign
// =============================================================================
//  « Pense toujours aux utilisateurs autres que nous » (Rodolf, le même jour).
//  Annuler, Rétablir, Enregistrer, Exporter, Grouper ne sont pas des verbes de
//  NKUIDesign : ce sont les verbes de TOUT éditeur. Les écrire dans une
//  application aurait donné un sixième dessin du même « Annuler » — le dépôt
//  porte déjà quatre jeux d'icônes pour cette raison exacte.
//
// =============================================================================
//  ⚠️ TRACÉ, JAMAIS UN ATLAS — et ce n'est pas une préférence
// =============================================================================
//  Un atlas impose un fichier, un chargement, un `texId`, une résolution, et il
//  devient flou dès qu'on change d'échelle. Ces glyphes vivent dans la BOÎTE
//  UNITÉ [0,1]² : `AddIcon` les met à l'échelle du rectangle qu'on lui donne, et
//  ils sont nets à 12 px comme à 64. C'est déjà la règle du kit (« aucun atlas :
//  tout se trace »), appliquée une fois de plus.
//
//  ⚠️ `filled` EXIGE UN CONTOUR CONVEXE (le triangulateur du kit est un éventail).
//     Une flèche n'est donc PAS un polygone rempli : c'est un TRAIT plus un
//     TRIANGLE. Les dessiner d'un seul contour concave donnerait une tache.
// -----------------------------------------------------------------------------

#include "NKGui/Core/NkGuiIcons.h"

namespace nkentseu {
	namespace editorkit {

		/// Construit (une fois) et pose le jeu standard. À appeler au démarrage,
		/// avant le premier montage d'un document qui porte des `icon`.
		///
		/// ⚠️ LE JEU EST `static` ET SA DURÉE DE VIE EST CELLE DU PROGRAMME, parce
		///    que `NkGuiPoserIcones` garde un POINTEUR. Un jeu construit sur la
		///    pile donnerait un pointeur pendant dès le retour — la même faute que
		///    le registre du kit a déjà payée (« le registre garde un pointeur »).
		inline void NkEditorPoserIconesStandard() noexcept {
			static nkgui::NkGuiIconSet s_jeu;
			static bool s_pret = false;
			if (s_pret) {
				nkgui::NkGuiPoserIcones(&s_jeu);
				return;
			}
			s_jeu.Reset(0u, 0, 0); // 100 % vectoriel : aucun atlas, aucune texture

			using nkgui::NkVec2;
			auto trait = [&](nkgui::NkGuiIconHandle h, const NkVec2 *p, int32 n, float32 e = 0.10f,
							 bool ferme = false) { s_jeu.AddContour(h, p, n, false, e, ferme); };
			auto plein = [&](nkgui::NkGuiIconHandle h, const NkVec2 *p, int32 n) {
				s_jeu.AddContour(h, p, n, true, 0.f, true);
			};
			// Un rectangle en TRAIT fermé — la brique de la moitié de ces dessins.
			auto cadre = [&](nkgui::NkGuiIconHandle h, float32 x, float32 y, float32 w,
							 float32 hh, float32 e = 0.09f) {
				const NkVec2 p[4] = {{x, y}, {x + w, y}, {x + w, y + hh}, {x, y + hh}};
				trait(h, p, 4, e, true);
			};
			auto boite = [&](nkgui::NkGuiIconHandle h, float32 x, float32 y, float32 w,
							 float32 hh) {
				const NkVec2 p[4] = {{x, y}, {x + w, y}, {x + w, y + hh}, {x, y + hh}};
				plein(h, p, 4);
			};

			// ── ANNULER / RÉTABLIR ──────────────────────────────────────────
			// Un trait qui remonte et part sur le côté, plus une pointe. Le sens
			// de lecture vient de la POINTE, pas de la courbe : à 14 px, une
			// courbe se lit à peine, une pointe se lit toujours.
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("annuler");
				const NkVec2 corps[4] = {{0.84f, 0.80f}, {0.84f, 0.62f}, {0.60f, 0.44f},
										 {0.34f, 0.44f}};
				trait(h, corps, 4, 0.10f, false);
				const NkVec2 pointe[3] = {{0.14f, 0.44f}, {0.38f, 0.28f}, {0.38f, 0.60f}};
				plein(h, pointe, 3);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("retablir"); // le miroir exact
				const NkVec2 corps[4] = {{0.16f, 0.80f}, {0.16f, 0.62f}, {0.40f, 0.44f},
										 {0.66f, 0.44f}};
				trait(h, corps, 4, 0.10f, false);
				const NkVec2 pointe[3] = {{0.86f, 0.44f}, {0.62f, 0.28f}, {0.62f, 0.60f}};
				plein(h, pointe, 3);
			}

			// ── GROUPER / DÉGROUPER ─────────────────────────────────────────
			// Deux carrés pleins, et ce qui les ENTOURE dit le verbe : un cadre
			// pour grouper, quatre coins écartés pour dégrouper.
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("grouper");
				cadre(h, 0.08f, 0.08f, 0.84f, 0.84f, 0.07f);
				boite(h, 0.24f, 0.24f, 0.24f, 0.24f);
				boite(h, 0.52f, 0.52f, 0.24f, 0.24f);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("degrouper");
				// Les quatre coins, séparés : le cadre s'est ouvert.
				const NkVec2 hg[3] = {{0.06f, 0.30f}, {0.06f, 0.06f}, {0.30f, 0.06f}};
				const NkVec2 hd[3] = {{0.70f, 0.06f}, {0.94f, 0.06f}, {0.94f, 0.30f}};
				const NkVec2 bg[3] = {{0.06f, 0.70f}, {0.06f, 0.94f}, {0.30f, 0.94f}};
				const NkVec2 bd[3] = {{0.70f, 0.94f}, {0.94f, 0.94f}, {0.94f, 0.70f}};
				trait(h, hg, 3, 0.09f, false);
				trait(h, hd, 3, 0.09f, false);
				trait(h, bg, 3, 0.09f, false);
				trait(h, bd, 3, 0.09f, false);
				boite(h, 0.38f, 0.38f, 0.24f, 0.24f);
			}

			// ── DUPLIQUER / COPIER / COLLER / COUPER ────────────────────────
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("dupliquer");
				cadre(h, 0.10f, 0.10f, 0.54f, 0.54f, 0.08f);
				cadre(h, 0.36f, 0.36f, 0.54f, 0.54f, 0.08f);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("copier");
				cadre(h, 0.08f, 0.08f, 0.52f, 0.62f, 0.08f);
				cadre(h, 0.40f, 0.30f, 0.52f, 0.62f, 0.08f);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("coller");
				cadre(h, 0.16f, 0.14f, 0.68f, 0.78f, 0.08f); // la planchette
				boite(h, 0.36f, 0.04f, 0.28f, 0.16f);		 // la pince
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("couper");
				const NkVec2 l1[2] = {{0.22f, 0.10f}, {0.72f, 0.74f}};
				const NkVec2 l2[2] = {{0.78f, 0.10f}, {0.28f, 0.74f}};
				trait(h, l1, 2, 0.08f, false);
				trait(h, l2, 2, 0.08f, false);
				boite(h, 0.14f, 0.74f, 0.20f, 0.20f);
				boite(h, 0.66f, 0.74f, 0.20f, 0.20f);
			}

			// ── ENREGISTRER (la disquette) ──────────────────────────────────
			// ⚠️ LA DISQUETTE N'EST PLUS UN OBJET CONNU DE PERSONNE, et c'est
			//    pourtant le glyphe le plus reconnu de « enregistrer » dans tous
			//    les éditeurs. On garde la convention : une icône n'a pas à être
			//    littérale, elle a à être APPRISE.
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("enregistrer");
				cadre(h, 0.10f, 0.10f, 0.80f, 0.80f, 0.09f);
				boite(h, 0.30f, 0.12f, 0.40f, 0.24f); // le volet
				boite(h, 0.24f, 0.54f, 0.52f, 0.34f); // l'étiquette
			}

			// ── EXPORTER / IMPORTER ─────────────────────────────────────────
			// Une boîte ouverte et une flèche. Le SENS de la flèche fait tout le
			// verbe : elle sort pour exporter, elle entre pour importer.
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("exporter");
				const NkVec2 bac[4] = {{0.12f, 0.52f}, {0.12f, 0.90f}, {0.88f, 0.90f},
									   {0.88f, 0.52f}};
				trait(h, bac, 4, 0.09f, false);
				const NkVec2 tige[2] = {{0.50f, 0.66f}, {0.50f, 0.26f}};
				trait(h, tige, 2, 0.10f, false);
				const NkVec2 pointe[3] = {{0.50f, 0.06f}, {0.30f, 0.30f}, {0.70f, 0.30f}};
				plein(h, pointe, 3);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("importer");
				const NkVec2 bac[4] = {{0.12f, 0.52f}, {0.12f, 0.90f}, {0.88f, 0.90f},
									   {0.88f, 0.52f}};
				trait(h, bac, 4, 0.09f, false);
				const NkVec2 tige[2] = {{0.50f, 0.06f}, {0.50f, 0.46f}};
				trait(h, tige, 2, 0.10f, false);
				const NkVec2 pointe[3] = {{0.50f, 0.66f}, {0.30f, 0.42f}, {0.70f, 0.42f}};
				plein(h, pointe, 3);
			}

			// ── NOUVEAU / OUVRIR / RECHARGER / SUPPRIMER ────────────────────
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("nouveau");
				const NkVec2 page[5] = {{0.20f, 0.06f}, {0.62f, 0.06f}, {0.80f, 0.26f},
										{0.80f, 0.94f}, {0.20f, 0.94f}};
				trait(h, page, 5, 0.08f, true);
				const NkVec2 pli[3] = {{0.62f, 0.06f}, {0.80f, 0.26f}, {0.62f, 0.26f}};
				plein(h, pli, 3);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("ouvrir");
				const NkVec2 pochette[6] = {{0.06f, 0.82f}, {0.06f, 0.22f}, {0.38f, 0.22f},
											{0.46f, 0.34f}, {0.94f, 0.34f}, {0.94f, 0.82f}};
				trait(h, pochette, 6, 0.08f, true);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("recharger");
				// Presque « rétablir », mais la boucle repart vers le bas : c'est
				// un retour à l'état du disque, pas un pas d'historique.
				const NkVec2 corps[4] = {{0.20f, 0.26f}, {0.72f, 0.26f}, {0.80f, 0.52f},
										 {0.56f, 0.80f}};
				trait(h, corps, 4, 0.10f, false);
				const NkVec2 pointe[3] = {{0.36f, 0.88f}, {0.62f, 0.62f}, {0.66f, 0.94f}};
				plein(h, pointe, 3);
			}
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("supprimer");
				const NkVec2 couvercle[2] = {{0.10f, 0.22f}, {0.90f, 0.22f}};
				trait(h, couvercle, 2, 0.09f, false);
				boite(h, 0.38f, 0.06f, 0.24f, 0.12f);
				const NkVec2 bac[4] = {{0.20f, 0.30f}, {0.80f, 0.30f}, {0.72f, 0.92f},
									   {0.28f, 0.92f}};
				trait(h, bac, 4, 0.09f, true);
			}

			// ⚠️ LE GLYPHE DE SECOURS EST UN CADRE VIDE, ET C'EST UN CHOIX : un
			//    nom d'icône inconnu doit SE VOIR. Sans secours, il ne dessinerait
			//    rien et se confondrait avec un bouton qui n'en demandait pas —
			//    une faute de frappe invisible, exactement ce que ce dépôt refuse.
			{
				const nkgui::NkGuiIconHandle h = s_jeu.AddPath("__inconnu__");
				cadre(h, 0.14f, 0.14f, 0.72f, 0.72f, 0.09f);
				const NkVec2 d1[2] = {{0.14f, 0.14f}, {0.86f, 0.86f}};
				trait(h, d1, 2, 0.07f, false);
				s_jeu.SetFallback(h);
			}

			s_pret = true;
			nkgui::NkGuiPoserIcones(&s_jeu);
		}

	} // namespace editorkit
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
