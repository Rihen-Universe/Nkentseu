#pragma once
// =============================================================================
// NkSyntheseSegments.h — LES SEGMENTS DE VUES en tete de l'ilot de gauche
// (apparence « Synthese », maquette D) : Fichiers, Recherche, Git, Jenga,
// Extensions, Collaboration — UNE vue a la fois. Chaque panneau de gauche les
// dessine en tete de son contenu ; un clic ouvre la vue a la place (meme
// feuille du dock), comme l'icone d'une barre d'activites.
//
// ⚠️ Separe de NkSynthese.h parce que l'Explorateur (inclus TRES tot) en a
//    besoin : il ne depend que de l'apparence, de NkUi et de la coquille.
// =============================================================================
#include "NKCode/Shell/NkApparence.h"
#include "NKEditorKit/NkEditorShell.h"
#include "NKEditorKit/NkEditorTooltip.h"

namespace nkentseu {
	namespace nkcode {

		/// Les vues de l'ilot de gauche, dans l'ordre de la maquette.
		struct NkSegmentVue {
				const char *panneau; ///< titre du panneau (cle de la coquille)
				const char *bulle;
				bool aVenir;		 ///< a sa place mais desactivee (« a venir »)
		};
		inline const NkSegmentVue *NkSegmentsVues(int32 &n) {
			static const NkSegmentVue k[] = {
				{"Explorateur", "Fichiers", false},
				{"Recherche", "Recherche", false},
				{"Controle de version", "Git", false},
				{"Jenga", "Jenga : projets du workspace", false},
				{"Extensions", "Extensions", false},
				{"Live Collab", "\xC3\x80 venir : la collaboration en direct", true},
			};
			n = 6;
			return k;
		}

		/// Les icones des segments (posees par le chargeur d'icones, NkAppIcons.h).
		inline uint32 *NkSegmentsIcones() {
			static uint32 t[6] = {0, 0, 0, 0, 0, 0};
			return t;
		}

		/// Ouvre `vue` dans la feuille de l'ilot de gauche et ferme ses voisines de
		/// feuille (la logique de OpenSideExclusive, sans dependre de Panels.h).
		inline void NkSegmentsOuvrir(editorkit::NkEditorShell *sh, const char *vue) {
			if (!sh || !vue)
				return;
			if (!sh->IsPanelOpen(vue) && sh->PanelDockNode(vue) >= 0)
				sh->DetachPanel(vue);
			sh->FocusPanel(vue);
			const int32 noeud = sh->PanelDockNode(vue);
			int32 n = 0;
			const NkSegmentVue *v = NkSegmentsVues(n);
			for (int32 i = 0; i < n; ++i) {
				const char *t = v[i].panneau;
				bool meme = true;
				for (const char *a = t, *b = vue;; ++a, ++b) {
					if (*a != *b) {
						meme = false;
						break;
					}
					if (!*a)
						break;
				}
				if (!meme && noeud >= 0 && sh->PanelDockNode(t) == noeud) {
					sh->ClosePanel(t);
					sh->DetachPanel(t);
				}
			}
		}

		/// La coquille, posee a chaque image par l'apparence (NkAppCommands.h) : les
		/// panneaux de gauche n'ont pas tous un pointeur sur elle.
		inline editorkit::NkEditorShell *&NkSegmentsShell() {
			static editorkit::NkEditorShell *sh = nullptr;
			return sh;
		}

		/// Hauteur de la rangee de segments (marges comprises), px ecran.
		inline float32 NkSegmentsHauteur(const NkGuiContext &ctx) {
			return ctx.S(10.f + 34.f + 6.f);
		}

		/// Dessine les segments en haut de `zone` (largeur de l'ilot) ; `actif` = le
		/// panneau qui les dessine. Rend la hauteur consommee.
		inline float32 NkSegmentsDessiner(NkGuiContext &ctx, const NkRect &zone, editorkit::NkEditorShell *sh,
										  const char *actif) {
			const NkApparencePalette &a = NkApparenceCourante().pal;
			NkGuiDrawList &dl = ctx.DL();
			const float32 S = ctx.S(1.f);
			const NkRect boite = {zone.x + 10.f * S, zone.y + 10.f * S, zone.w - 20.f * S, 34.f * S};
			dl.AddRectFilled(boite, a.haut, 9.f * S);
			int32 n = 0;
			const NkSegmentVue *v = NkSegmentsVues(n);
			const float32 w = (boite.w - 6.f * S - (float32)(n - 1) * 2.f * S) / (float32)n;
			const NkVec2 m = ctx.input.mousePos;
			const bool atteint = ctx.PointReachable(m) && ctx.popupDepth == 0;
			for (int32 i = 0; i < n; ++i) {
				const NkRect r = {boite.x + 3.f * S + (float32)i * (w + 2.f * S), boite.y + 3.f * S, w, 28.f * S};
				bool estActif = actif != nullptr;
				for (const char *p = v[i].panneau, *q = actif; estActif; ++p, ++q) {
					if (*p != *q)
						estActif = false;
					if (!*p || !*q)
						break;
				}
				const bool hov = atteint && NkGuiRectContains(r, m) && !v[i].aVenir;
				if (estActif) {
					dl.AddRectFilled(r, a.panneau, 7.f * S);
					dl.AddRect(r, a.trait, 1.f, 7.f * S);
				} else if (hov)
					dl.AddRectFilled(r, a.survol, 7.f * S);
				const NkColor c = estActif ? a.fg : (v[i].aVenir ? NkMelange(a.haut, a.fg3, 0.55f) : (hov ? a.fg2 : a.fg3));
				const uint32 tex = NkSegmentsIcones()[i];
				const float32 is = 17.f * S;
				if (tex)
					dl.AddImage(tex, {r.x + (r.w - is) * 0.5f, r.y + (r.h - is) * 0.5f, is, is}, {0, 0}, {1, 1}, c);
				editorkit::NkTooltip(ctx, atteint && NkGuiRectContains(r, m), v[i].bulle);
				if (hov && ctx.input.mouseClicked[0] && !estActif) {
					NkSegmentsOuvrir(sh ? sh : NkSegmentsShell(), v[i].panneau);
					ctx.input.mouseClicked[0] = false;
				}
			}
			return NkSegmentsHauteur(ctx);
		}

		/// En tete d'un panneau de gauche (flux NKGui) : dessine les segments s'il le
		/// faut et RESERVE leur hauteur. Rend vrai si la Synthese est active.
		inline bool NkSegmentsEnTete(NkGuiContext &ctx, const char *actif) {
			if (!NkApparenceCourante().dispo.synthese)
				return false;
			const NkRect clip = ctx.DL().CurrentClip();
			const float32 h = NkSegmentsHauteur(ctx);
			NkSegmentsDessiner(ctx, {clip.x, clip.y, clip.w, h}, NkSegmentsShell(), actif);
			ctx.NextItemRect(ctx.ContentWidth(), h - (ctx.layout.cursor.y - clip.y > 0.f ? ctx.layout.cursor.y - clip.y : 0.f));
			return true;
		}

	} // namespace nkcode
} // namespace nkentseu
