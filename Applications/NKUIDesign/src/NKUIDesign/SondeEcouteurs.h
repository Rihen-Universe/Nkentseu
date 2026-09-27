#pragma once
// -----------------------------------------------------------------------------
// @File    SondeEcouteurs.h
// @Brief   `--sonde-ecouteurs` : un evenement part-il du bon widget, avec les
//          bonnes coordonnees, et un identifiant mal ecrit se fait-il NOMMER ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ ELLE N'INJECTE RIEN SUR LA MACHINE, ET IL FAUT LE LIRE AVANT DE LA MODIFIER
// =============================================================================
//  La regle permanente de ce depot est sans exception : AUCUNE injection d'entree
//  sur la machine de Rodolf -- ni souris, ni clavier. Cette sonde ne l'enfreint
//  pas : elle ECRIT DES CHAMPS dans une structure `NkGuiInput` qui vit en
//  memoire, dans son propre contexte. Aucun appel systeme, aucun evenement
//  envoye a la fenetre de qui que ce soit, aucune fenetre ouverte.
//
//  Si un jour quelqu'un est tente de la faire piloter une vraie fenetre pour
//  « tester en vrai » : c'est exactement ce que la regle interdit, et la sonde
//  n'en a pas besoin -- l'entree est une donnee, elle se fabrique.
//
// =============================================================================
//  CE QU'ELLE DEMANDE
// =============================================================================
//   C1  ENTRE une fois, et une seule. Un survol qui se rejoue a chaque image est
//       un defaut qu'on ne voit pas ; une animation d'entree qui ne demarre
//       jamais est un defaut qu'on voit. Les deux viennent du meme oubli.
//   C2  BOUGE porte le bon deplacement.
//   C3  APPUI et RELACHE, avec le bon bouton.
//   C4  MOLETTE porte (wheelH, wheel) dans cet ordre.
//   C5  🔴 `local` == `ecran` - le coin de la zone. C'est la promesse des deux
//       reperes : si elle est fausse, chaque ecouteur refait la soustraction et
//       celui qui la rate se trompe EN SILENCE.
//   C6  SORTI arrive meme quand le pointeur est deja loin -- sans lui, un widget
//       garde son survol pour toujours des que la souris sort vite.
//   C7  Un identifiant MAL ECRIT se fait nommer : `sansDestinataire` ET le nom.
//
//  NEGATIF : aucune table posee -> rien n'est servi, et `evenementsExposes`
//  reste a zero. Il prouve que les criteres ci-dessus mesurent bien le chemin
//  d'exposition, et pas autre chose qui serait vrai de toute facon.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDEECOUTEURS_H__
#define __NKENTSEU_NKUIDESIGN_SONDEECOUTEURS_H__

#include "NKGui/Doc/NkGuiEcouteurs.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkuidesign {

	namespace sondeecout {

		using nkentseu::NkArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using namespace nkentseu::nkgui;

		/// Un seul widget, a une place CONNUE : `pos` + `size` posés, donc son
		/// rectangle ne depend d'aucun agencement -- et les attendus de la sonde ne
		/// se perimeront pas si le theme change ses marges.
		inline const char *NkEDoc() {
			return "nkgui 0.4\n"
				   "\n"
				   "widgets \"ecoute\" {\n"
				   "  Host \"toile\"  { pos = (100, 50), size = (200, 120) }\n"
				   "}\n";
		}

		/// Ce que l'ecouteur a vu. Un compte PAR TYPE : un total ne dirait pas
		/// lequel manque.
		struct Vu {
				uint32 entre = 0, sorti = 0, bouge = 0, appui = 0, relache = 0;
				uint32 molette = 0, doubleClic = 0;
				NkVec2 dernierLocal{0.f, 0.f}, dernierEcran{0.f, 0.f};
				NkVec2 dernierDelta{0.f, 0.f};
				int32 dernierBouton = -1;
				NkRect dernierRect{0.f, 0.f, 0.f, 0.f};
		};

		inline Vu &NkEVu() {
			static Vu v;
			return v;
		}

		inline bool NkEEcouteur(const NkGuiEvtDetail &e, void *) noexcept {
			Vu &v = NkEVu();
			v.dernierLocal = e.local;
			v.dernierEcran = e.ecran;
			v.dernierRect = e.rect;
			switch (e.type) {
				case NkGuiEvenement::PointerEnter: ++v.entre; break;
				case NkGuiEvenement::PointerLeave: ++v.sorti; break;
				case NkGuiEvenement::PointerMove:
					++v.bouge;
					v.dernierDelta = e.delta;
					break;
				case NkGuiEvenement::PointerDown:
					++v.appui;
					v.dernierBouton = e.bouton;
					break;
				case NkGuiEvenement::PointerUp: ++v.relache; break;
				case NkGuiEvenement::DoubleClick: ++v.doubleClic; break;
				case NkGuiEvenement::Wheel:
					++v.molette;
					v.dernierDelta = e.delta;
					break;
				default: break;
			}
			return false; // on ne consomme pas : la sonde observe, elle n'agit pas
		}

		/// Monte le document une fois, avec l'entree telle qu'elle est posee.
		inline void NkEImage(NkGuiContext &ctx, NkGuiMonteRapport &rap) noexcept {
			NkArchive doc;
			nkentseu::NkGuiDiag diag;
			const char *src = NkEDoc();
			uint32 n = 0;
			while (src[n] != '\0') ++n;
			if (!NkGuiArchive::Read(src, n, doc, diag))
				return;
			ctx.viewW = 640;
			ctx.viewH = 480;
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(NkRect{0.f, 0.f, 640.f, 480.f});
			ctx.DL().Reset();
			NkGuiMonteEtat etat;
			NkGuiMonteur::Monter(ctx, doc, "ecoute", etat, rap, nullptr);
			ctx.EndFrame();
		}

	} // namespace sondeecout

	/// `--sonde-ecouteurs` : 0 si tout passe, 1 sinon.
	inline int SondeEcouteurs() noexcept {
		using namespace sondeecout;
		printf("=== SONDE ECOUTEURS — un evenement part-il du bon widget ? ===\n");
		printf("  (aucune injection sur la machine : l'entree est ecrite en memoire)\n");
		uint32 ko = 0u;

		// Le rectangle attendu vient du document : pos (100,50), size (200,120).
		const NkRect attendu{100.f, 50.f, 200.f, 120.f};

		// ── LE NEGATIF D'ABORD : AUCUNE TABLE POSEE ────────────────────────
		// Avant tout branchement, pour qu'il ne puisse pas beneficier d'un etat
		// laisse par un cas precedent.
		{
			NkGuiPoserEcouteurs(nullptr);
			NkEVu() = Vu();
			NkGuiContext ctx;
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkGuiMonteRapport rap;
			NkEImage(ctx, rap);
			const bool ok = (rap.evenementsExposes == 0u) && (NkEVu().entre == 0u);
			if (!ok) ++ko;
			printf("  NEG sans table     exposes=%u entre=%u  %s\n", rap.evenementsExposes,
				   NkEVu().entre, ok ? "OK (rien ne part)" : "<<< KO");
		}

		NkGuiEcouteurs table;
		(void)table.Brancher("toile", &NkEEcouteur, nullptr);
		NkGuiPoserEcouteurs(&table);

		// ── C1 : ENTRE UNE FOIS, PUIS PLUS ─────────────────────────────────
		{
			NkEVu() = Vu();
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			// Image 1 : le pointeur est dedans, sans position precedente.
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkEImage(ctx, rap);
			const uint32 e1 = NkEVu().entre;
			// Image 2 : il n'a pas bouge. `Entre` ne doit PAS se rejouer.
			NkEImage(ctx, rap);
			const uint32 e2 = NkEVu().entre;
			const bool ok = (e1 == 1u) && (e2 == 1u);
			if (!ok) ++ko;
			printf("  C1 entre           image 1 = %u, image 2 = %u (attendu 1 et 1)  %s\n", e1,
				   e2, ok ? "OK" : "<<< KO");
		}

		// ── C2 et C5 : BOUGE, ET LES DEUX REPERES ──────────────────────────
		{
			NkEVu() = Vu();
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkEImage(ctx, rap); // entre
			ctx.input.mousePos = NkVec2(170.f, 100.f);
			NkEImage(ctx, rap); // bouge de (20, 10)
			const Vu &v = NkEVu();
			const bool c2 = v.bouge >= 1u && v.dernierDelta.x > 19.f && v.dernierDelta.x < 21.f
							&& v.dernierDelta.y > 9.f && v.dernierDelta.y < 11.f;
			// 🔴 C5 : `local` == `ecran` - coin de la zone, et le RECTANGLE remis est
			//    bien celui du document. Un ecouteur qui n'aurait que l'ecran devrait
			//    faire cette soustraction lui-meme.
			const bool c5 = v.dernierRect.x > 99.f && v.dernierRect.x < 101.f
							&& v.dernierRect.w > 199.f && v.dernierRect.w < 201.f
							&& v.dernierLocal.x > 69.f && v.dernierLocal.x < 71.f
							&& v.dernierLocal.y > 49.f && v.dernierLocal.y < 51.f;
			if (!c2) ++ko;
			if (!c5) ++ko;
			printf("  C2 bouge           n=%u delta=(%.1f, %.1f) attendu (20,0 ; 10,0)  %s\n",
				   v.bouge, (double)v.dernierDelta.x, (double)v.dernierDelta.y,
				   c2 ? "OK" : "<<< KO");
			printf("  C5 deux reperes    rect=(%.0f, %.0f, %.0f, %.0f) local=(%.1f, %.1f) "
				   "attendu local (70,0 ; 50,0)  %s\n",
				   (double)v.dernierRect.x, (double)v.dernierRect.y, (double)v.dernierRect.w,
				   (double)v.dernierRect.h, (double)v.dernierLocal.x, (double)v.dernierLocal.y,
				   c5 ? "OK" : "<<< KO");
			if (!c5)
				printf("       ^ rect attendu (%.0f, %.0f, %.0f, %.0f) depuis `pos` et `size` du document.\n",
					   (double)attendu.x, (double)attendu.y, (double)attendu.w, (double)attendu.h);
		}

		// ── C3 : APPUI ET RELACHE, AVEC LE BON BOUTON ──────────────────────
		{
			NkEVu() = Vu();
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkEImage(ctx, rap);
			// `mouseClicked` est derive par `NewFrame` depuis `mouseDown`/`mousePrev` :
			// on pose donc le bouton ENFONCE et on laisse le contexte deriver.
			ctx.input.mouseDown[1] = true; // le bouton DROIT, pas le gauche : un
										   // attendu a 0 passerait par hasard.
			NkEImage(ctx, rap);
			const uint32 appui = NkEVu().appui;
			const int32 bt = NkEVu().dernierBouton;
			ctx.input.mouseDown[1] = false;
			NkEImage(ctx, rap);
			const uint32 rel = NkEVu().relache;
			const bool ok = (appui == 1u) && (bt == 1) && (rel == 1u);
			if (!ok) ++ko;
			printf("  C3 appui/relache   appui=%u bouton=%d relache=%u (attendu 1, 1, 1)  %s\n",
				   appui, bt, rel, ok ? "OK" : "<<< KO");
		}

		// ── C4 : LA MOLETTE, DANS LE BON ORDRE ─────────────────────────────
		{
			NkEVu() = Vu();
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkEImage(ctx, rap);
			ctx.input.wheel = 3.f;	// vertical -> y
			ctx.input.wheelH = -2.f; // horizontal -> x
			NkEImage(ctx, rap);
			const Vu &v = NkEVu();
			// L'ORDRE EST LE CRITERE : (wheelH, wheel). Inverse, il passerait un test
			// qui ne regarderait que « la molette est arrivee ».
			const bool ok = v.molette >= 1u && v.dernierDelta.x < -1.9f && v.dernierDelta.x > -2.1f
							&& v.dernierDelta.y > 2.9f && v.dernierDelta.y < 3.1f;
			if (!ok) ++ko;
			printf("  C4 molette         n=%u delta=(%.1f, %.1f) attendu (-2,0 ; 3,0)  %s\n",
				   v.molette, (double)v.dernierDelta.x, (double)v.dernierDelta.y,
				   ok ? "OK" : "<<< KO");
		}

		// ── C6 : SORTI, MEME LOIN ──────────────────────────────────────────
		{
			NkEVu() = Vu();
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkEImage(ctx, rap);
			ctx.input.mousePos = NkVec2(600.f, 400.f); // tres loin de la zone
			NkEImage(ctx, rap);
			const bool ok = NkEVu().sorti == 1u;
			if (!ok) ++ko;
			printf("  C6 sorti           n=%u (attendu 1, pointeur a 600x400)  %s\n",
				   NkEVu().sorti, ok ? "OK" : "<<< KO");
		}

		// ── C7 : UN IDENTIFIANT MAL ECRIT SE FAIT NOMMER ───────────────────
		{
			NkGuiEcouteurs faute;
			(void)faute.Brancher("toilee", &NkEEcouteur, nullptr); // un `e` de trop
			NkGuiPoserEcouteurs(&faute);
			NkEVu() = Vu();
			NkGuiContext ctx;
			ctx.input.mousePos = NkVec2(150.f, 90.f);
			NkGuiMonteRapport rap;
			NkEImage(ctx, rap);
			const bool ok = faute.sansDestinataire >= 1u
							&& faute.dernierSansDestinataire.Compare("toile") == 0
							&& NkEVu().entre == 0u;
			if (!ok) ++ko;
			printf("  C7 nom introuvable sansDestinataire=%u dernier=\"%s\"  %s\n",
				   faute.sansDestinataire, faute.dernierSansDestinataire.CStr(),
				   ok ? "OK" : "<<< KO");
			printf("       ^ c'est ce nom qui transforme « il manque quelque chose » en\n");
			printf("         « tu as branche toilee ».\n");
		}

		NkGuiPoserEcouteurs(nullptr);
		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDEECOUTEURS_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
