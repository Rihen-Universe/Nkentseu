#pragma once
// -----------------------------------------------------------------------------
// @File    SondeToile.h
// @Brief   `--sonde-toile` : le role `Canvas` tient-il son cadrage, et zoome-t-il
//          AU CURSEUR ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LE CRITERE QUI DECIDE : C2
// =============================================================================
//  🔴 ZOOMER AU CURSEUR, C'EST « LE POINT DU MONDE SOUS LE CURSEUR NE BOUGE PAS ».
//     C'est la seule formulation qui se mesure, et c'est la seule qui distingue
//     une toile agreable d'une toile penible. Zoomer au centre -- ce qu'on ecrit
//     quand on va vite -- fait fuir le point qu'on regarde, et ca passe TOUS les
//     criteres de comptage : la toile s'affiche, le zoom change, rien ne rougit.
//
//     Le negatif `NK_TOILE_MUTATION=centre` existe pour ca.
//
//  ⚠️ ELLE N'INJECTE RIEN SUR LA MACHINE. Comme `SondeEcouteurs`, elle ecrit des
//     champs dans une `NkGuiInput` en memoire. Aucun appel systeme, aucune
//     fenetre. La regle permanente est sans exception.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDETOILE_H__
#define __NKENTSEU_NKUIDESIGN_SONDETOILE_H__

#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkuidesign {

	namespace sondetoile {

		using nkentseu::NkArchive;
		using nkentseu::float32;
		using nkentseu::uint32;
		using namespace nkentseu::nkgui;

		inline const char *NkTDoc() {
			return "nkgui 0.4\n"
				   "\n"
				   "widgets \"toile\" {\n"
				   "  Canvas \"design.toile\"  { pos = (40, 20), size = (400, 300) }\n"
				   "}\n";
		}

		/// La zone attendue, lue dans le document -- pas devinee.
		inline NkRect NkTZone() {
			return NkRect{40.f, 20.f, 400.f, 300.f};
		}

		/// Un hote qui remplit, pour que la toile ne soit pas comptee vide, et qui
		/// RETIENT la vue qu'on lui remet : c'est par lui qu'on la mesure.
		struct HoteToile : public NkGuiMonteHooks {
				NkGuiVue derniere;
				uint32 appels = 0;
				bool RemplirToile(NkGuiContext &ctx, const char *nom, const NkRect &zone,
								  const NkGuiVue &vue) noexcept override {
					(void)ctx;
					(void)nom;
					(void)zone;
					derniere = vue;
					++appels;
					return true;
				}
		};

		/// Monte une image. `etat` est passe par REFERENCE et survit d'un appel a
		/// l'autre : c'est ce qui permet de mesurer que le cadrage persiste.
		inline void NkTImage(NkGuiContext &ctx, NkGuiMonteEtat &etat, NkGuiMonteRapport &rap,
							 HoteToile *hote) noexcept {
			NkArchive doc;
			nkentseu::NkGuiDiag diag;
			const char *src = NkTDoc();
			uint32 n = 0;
			while (src[n] != '\0') ++n;
			if (!NkGuiArchive::Read(src, n, doc, diag))
				return;
			// 🔴 `Preparer` D'ABORD, ET SON OUBLI A COUTE DEUX CRITERES ROUGES.
			//    Les entrees d'etat ne naissent pas au montage : elles sont
			//    DECLAREES par une phase 1 (`PreparerCorps` -> `etat.Declarer`), et
			//    `etat.Get` rend `nullptr` pour ce qui n'a pas ete declare. Sans cet
			//    appel, la toile n'avait pas d'entree, donc pas de cadrage, donc
			//    aucun geste -- et le diagnostic evident (« les gestes sont casses »)
			//    aurait envoye corriger du code parfaitement sain.
			//    `NkGuiCoquille` l'appelle, elle : c'est la sonde qui s'en ecartait.
			NkGuiMonteur::Preparer(doc, etat);
			ctx.viewW = 640;
			ctx.viewH = 480;
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(NkRect{0.f, 0.f, 640.f, 480.f});
			ctx.DL().Reset();
			NkGuiMonteur::Monter(ctx, doc, "toile", etat, rap, hote);
			ctx.EndFrame();
		}

		inline bool Proche(float32 a, float32 b, float32 tol) noexcept {
			const float32 d = a - b;
			return (d < 0.f ? -d : d) <= tol;
		}

	} // namespace sondetoile

	/// `--sonde-toile` : 0 si tout passe, 1 sinon.
	inline int SondeToile() noexcept {
		using namespace sondetoile;
		printf("=== SONDE TOILE — le role `Canvas` et son cadrage ===\n");
		printf("  (aucune injection sur la machine : l'entree est ecrite en memoire)\n");
		uint32 ko = 0u;
		const NkRect zone = NkTZone();

		// ── C1 : ELLE MONTE, ET UNE TOILE VIDE SE SIGNALE ──────────────────
		{
			NkGuiContext ctx;
			NkGuiMonteEtat etat;
			NkGuiMonteRapport rap;
			NkTImage(ctx, etat, rap, nullptr); // AUCUN hote : elle doit se signaler
			uint32 traits = 0;
			for (uint32 i = 0; i < (uint32)ctx.DL().cmds.Size(); ++i)
				if (ctx.DL().cmds[i].texId == 0u) ++traits;
			const bool ok = (rap.toiles == 1u) && (rap.toilesNonRemplies == 1u) && traits > 0u;
			if (!ok) ++ko;
			printf("  C1 montee/vide     toiles=%u nonRemplies=%u traits=%u  %s\n", rap.toiles,
				   rap.toilesNonRemplies, traits, ok ? "OK" : "<<< KO");
		}

		// ── C2 : 🔴 LE ZOOM AU CURSEUR ─────────────────────────────────────
		{
			HoteToile hote;
			NkGuiContext ctx;
			NkGuiMonteEtat etat;
			NkGuiMonteRapport rap;
			// Un curseur DELIBEREMENT loin du centre : au centre, zoomer au centre
			// et zoomer au curseur donnent le meme resultat, et le critere ne
			// refuterait rien.
			const NkVec2 curseur(zone.x + 60.f, zone.y + 40.f);
			ctx.input.mousePos = curseur;
			NkTImage(ctx, etat, rap, &hote);
			const NkGuiVue avant = hote.derniere;
			const NkVec2 mondeAvant = avant.VersMonde(curseur, zone);

			ctx.input.wheel = 1.f; // un cran vers l'avant
			NkTImage(ctx, etat, rap, &hote);
			const NkGuiVue apres = hote.derniere;
			const NkVec2 mondeApres = apres.VersMonde(curseur, zone);

			const bool aZoome = apres.zoom > avant.zoom + 0.001f;
			const bool immobile = Proche(mondeAvant.x, mondeApres.x, 0.01f)
								  && Proche(mondeAvant.y, mondeApres.y, 0.01f);
			const bool ok = aZoome && immobile && rap.toilesZoomees >= 1u;
			if (!ok) ++ko;
			printf("  C2 zoom au curseur zoom %.3f -> %.3f | monde sous le curseur "
				   "(%.3f, %.3f) -> (%.3f, %.3f)  %s\n",
				   (double)avant.zoom, (double)apres.zoom, (double)mondeAvant.x,
				   (double)mondeAvant.y, (double)mondeApres.x, (double)mondeApres.y,
				   ok ? "OK" : "<<< KO");
			if (!ok && aZoome)
				printf("       ^ le point a FUI : c'est un zoom au centre, pas au curseur.\n");
		}

		// ── C3 : LE DEPLACEMENT, ET SON ECHELLE ────────────────────────────
		{
			HoteToile hote;
			NkGuiContext ctx;
			NkGuiMonteEtat etat;
			NkGuiMonteRapport rap;
			ctx.input.mousePos = NkVec2(zone.x + 100.f, zone.y + 100.f);
			NkTImage(ctx, etat, rap, &hote);
			const NkGuiVue avant = hote.derniere;
			ctx.input.mouseDown[2] = true; // le bouton du MILIEU
			ctx.input.mousePos = NkVec2(zone.x + 130.f, zone.y + 120.f); // +30, +20
			NkTImage(ctx, etat, rap, &hote);
			const NkGuiVue apres = hote.derniere;
			// Le centre recule de delta/zoom : tirer la toile vers la droite fait
			// regarder plus a GAUCHE. Le signe est le critere -- inverse, la toile
			// part dans le mauvais sens et tous les comptes restent verts.
			const bool ok = Proche(apres.centre.x, avant.centre.x - 30.f / avant.zoom, 0.01f)
							&& Proche(apres.centre.y, avant.centre.y - 20.f / avant.zoom, 0.01f)
							&& rap.toilesDeplacees >= 1u;
			if (!ok) ++ko;
			printf("  C3 deplacement     centre (%.2f, %.2f) -> (%.2f, %.2f) attendu "
				   "(%.2f, %.2f)  %s\n",
				   (double)avant.centre.x, (double)avant.centre.y, (double)apres.centre.x,
				   (double)apres.centre.y, (double)(avant.centre.x - 30.f / avant.zoom),
				   (double)(avant.centre.y - 20.f / avant.zoom), ok ? "OK" : "<<< KO");
		}

		// ── C4 : LE CADRAGE SURVIT A L'IMAGE ───────────────────────────────
		{
			HoteToile hote;
			NkGuiContext ctx;
			NkGuiMonteEtat etat;
			NkGuiMonteRapport rap;
			ctx.input.mousePos = NkVec2(zone.x + 60.f, zone.y + 40.f);
			NkTImage(ctx, etat, rap, &hote);
			ctx.input.wheel = 1.f;
			NkTImage(ctx, etat, rap, &hote);
			const float32 zoomApresGeste = hote.derniere.zoom;
			// Deux images SANS aucun geste : le cadrage ne doit pas revenir.
			ctx.input.wheel = 0.f;
			NkTImage(ctx, etat, rap, &hote);
			NkTImage(ctx, etat, rap, &hote);
			const bool ok = Proche(hote.derniere.zoom, zoomApresGeste, 0.0001f);
			if (!ok) ++ko;
			printf("  C4 persistance     zoom %.3f apres le geste, %.3f deux images plus "
				   "tard  %s\n",
				   (double)zoomApresGeste, (double)hote.derniere.zoom, ok ? "OK" : "<<< KO");
		}

		// ── C5 : LES DEUX TRANSFORMATIONS SONT INVERSES ────────────────────
		{
			// Sans ce critere, C2 pourrait passer avec deux fonctions fausses de la
			// meme facon : elles se compenseraient, et tout clic tomberait a cote.
			NkGuiVue v;
			v.centre = NkVec2(12.5f, -7.25f);
			v.zoom = 2.5f;
			const NkVec2 monde(33.f, 41.f);
			const NkVec2 ecran = v.VersEcran(monde, zone);
			const NkVec2 retour = v.VersMonde(ecran, zone);
			const bool ok = Proche(retour.x, monde.x, 0.001f) && Proche(retour.y, monde.y, 0.001f);
			if (!ok) ++ko;
			printf("  C5 aller-retour    (%.2f, %.2f) -> ecran -> (%.2f, %.2f)  %s\n",
				   (double)monde.x, (double)monde.y, (double)retour.x, (double)retour.y,
				   ok ? "OK" : "<<< KO");
		}

		// ── C6 : LA GRILLE GARDE UNE DENSITE LISIBLE ───────────────────────
		{
			// Un pas fixe en unites de monde donnerait, a faible zoom, des milliers
			// de traits, et a fort zoom, aucun. On mesure aux deux extremes.
			uint32 nBas = 0, nHaut = 0;
			for (int32 passe = 0; passe < 2; ++passe) {
				HoteToile hote;
				NkGuiContext ctx;
				NkGuiMonteEtat etat;
				NkGuiMonteRapport rap;
				ctx.input.mousePos = NkVec2(zone.x + 10.f, zone.y + 10.f);
				NkTImage(ctx, etat, rap, &hote);
				// On force le zoom par l'etat, comme le ferait un « cadrer sur ».
				// ⚠️ PAR SON NOM, pas en balayant la table : viser `design.toile`
				//    dit ce qu'on regle. Une boucle sur toutes les entrees reglerait
				//    aussi celles des autres widgets -- juste ici par hasard, faux
				//    des qu'un second widget entre dans le document.
				if (NkGuiMonteEtat::Entree *en = etat.Get(NkStringView("design.toile")))
					en->f = (passe == 0) ? 0.05f : 40.f;
				NkGuiContext ctx2;
				ctx2.input.mousePos = ctx.input.mousePos;
				NkGuiMonteRapport rap2;
				NkTImage(ctx2, etat, rap2, &hote);
				// 🔴 ON COMPTE LES SOMMETS, PAS LES COMMANDES. Premiere version :
				//    les commandes -- et elle rendait « 1 » aux DEUX zooms, parce que
				//    la draw-list fusionne des centaines de lignes en une seule
				//    commande. Le critere passait au vert sans rien mesurer : il ne
				//    pouvait pas echouer, donc il ne prouvait rien.
				const uint32 sommets = (uint32)ctx2.DL().vtx.Size();
				if (passe == 0) nBas = sommets; else nHaut = sommets;
			}
			// LA VRAIE AFFIRMATION : les deux densites restent COMPARABLES. Un pas
			// fixe en unites de monde donnerait, sur une zone de 400 px, environ
			// 800 lignes a zoom 0,05 et UNE SEULE a zoom 40 -- un facteur de plusieurs
			// centaines. Un facteur borne est donc exactement ce que « le pas s'adapte »
			// veut dire, et c'est refutable.
			const uint32 grand = nBas > nHaut ? nBas : nHaut;
			const uint32 petit = nBas < nHaut ? nBas : nHaut;
			const bool ok = petit > 0u && grand <= petit * 8u;
			if (!ok) ++ko;
			printf("  C6 grille adaptee  sommets a zoom 0,05 = %u ; a zoom 40 = %u "
				   "(facteur %.1f, max 8)  %s\n",
				   nBas, nHaut, petit > 0u ? (double)grand / (double)petit : 0.0,
				   ok ? "OK" : "<<< KO");
		}

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDETOILE_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
