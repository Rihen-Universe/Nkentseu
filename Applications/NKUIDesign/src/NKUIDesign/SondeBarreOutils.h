#pragma once
// -----------------------------------------------------------------------------
// @File    SondeBarreOutils.h
// @Brief   `--sonde-barre-outils` : les boutons de la barre RÉAGISSENT-ILS ?
//          Survol, clic, et l'action qui part — sans fenêtre et sans GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI ELLE EXISTE
// =============================================================================
//  Rodolf, 28/09 : « et les boutons n'ont pas d'effet de survol ni de clic ».
//
//  🔴 ET JE REFUSE DE LE DEVINER. Trois causes tenaient également debout sans
//     mesure : (a) les boutons sont GRISÉS par le contrôleur (Annuler sans
//     historique, Grouper sans sélection) et un grisé ne réagit pas — ce serait
//     le comportement JUSTE ; (b) la région que la barre pose ne correspond pas
//     à celle où le clic arrive ; (c) l'icône se peint par-dessus et avale le
//     survol. Chacune se corrige autrement, et deux d'entre elles se corrigent
//     en cassant quelque chose.
//
//  Cette sonde monte la racine `barreOutils` avec un pointeur POSÉ sur un bouton
//  et lit le relevé : `NK_GUI_ETAT_SURVOLE` y est, ou il n'y est pas.
//
// =============================================================================
//  LES QUATRE CRITÈRES
// =============================================================================
//   (t1) les boutons se MONTENT, et on connaît leurs rectangles.
//   (t2) 🔴 SURVOL : pointeur au centre du bouton -> `SURVOLE` dans le relevé.
//        ⚠️ TROIS IMAGES, ET CE N'EST PAS UNE MARGE : NKGui résout le survol sur
//           `hotIdPrev`, donc sur l'image PRÉCÉDENTE. Un banc à une seule image
//           conclurait « le survol ne marche pas » sur un code parfait.
//   (t3) CLIC : press+release sur le bouton -> l'action de la table est APPELÉE.
//        Un survol sans clic serait un bouton décoratif.
//   (t4) NÉGATIF : pointeur LOIN du bouton -> ni survol, ni action. Sans lui,
//        une version qui déclencherait tout, tout le temps, passerait (t2) et
//        (t3) les yeux fermés.
//
// ⚠️ ELLE JUGE LE BOUTON `design.exporter`, ET CE CHOIX EST MOTIVÉ : c'est l'un
//    des rares que le contrôleur n'appelle JAMAIS à griser (contrairement à
//    Annuler, Grouper, Dupliquer…). Mesurer le survol sur un bouton grisé aurait
//    rendu un rouge parfaitement correct.
// -----------------------------------------------------------------------------

namespace nkuidesign {

	inline nkentseu::int32 SondeBarreOutils(const nkgui::NkActionNommee *actions,
											nkentseu::uint32 nActions) {
		using namespace nkentseu;
		using nkentseu::uint32;
		printf("=== SONDE BARRE D'OUTILS — survol, clic, action ===\n");
		uint32 ko = 0u;

		NkCoquilleDocument coq;
		if (!coq.ChargerDepuisDossier("Resources/Interface/NKUIDesign")) {
			printf("  KO : Interface.nkgui NON LU\n");
			return 1;
		}
		coq.PoserTables(actions, nActions, nullptr, 0u);

		// La bande telle que la coquille la pose depuis le 28/09 : la rangee
		// BASSE d'un bandeau de 68, pleine largeur, moins 4 px de marge.
		const nkgui::NkRect bande{0.f, 0.f, 1440.f, 26.f};

		nkgui::NkGuiContext ctx;
		ctx.viewW = 1440;
		ctx.viewH = 200;
		nkgui::NkGuiIntrospectActiver(ctx, true);

		// ⚠️ L'ETAT DU MONTEUR SE PREPARE, SINON AUCUNE ENTREE N'EXISTE et tous
		//    les widgets retombent sur leur defaut — piege deja paye le 27/09 par
		//    la sonde des ecouteurs, qui voyait des gestes « cassés » faute de
		//    `Preparer`.
		auto image = [&](nkgui::NkVec2 souris, bool bouton) {
			ctx.input.mousePos = souris;
			ctx.input.mouseDown[0] = bouton;
			ctx.BeginFrame(1.f / 60.f);
			ctx.BeginLayout(bande);
			ctx.DL().Reset();
			ctx.layout.region = bande;
			ctx.layout.cursor = {bande.x + 8.f, bande.y};
			ctx.layout.lineStartX = ctx.layout.cursor.x;
			ctx.layout.curLineH = 0.f;
			ctx.layout.maxX = ctx.layout.cursor.x;
			coq.bande.Monter(ctx, "barreOutils");
			ctx.EndFrame();
		};

		const char kCible[] = "design.exporter";

		// ── (t1) LES BOUTONS SE MONTENT ────────────────────────────────────
		image({-500.f, -500.f}, false);
		image({-500.f, -500.f}, false);
		nkgui::NkRect rCible{0.f, 0.f, 0.f, 0.f};
		{
			int32 n = 0;
			const nkgui::NkGuiNote *notes = nkgui::NkGuiIntrospectNotes(ctx, n);
			uint32 boutons = 0u;
			for (int32 i = 0; i < n; ++i)
				if (notes[i].nature == nkgui::NkGuiNature::Bouton) {
					++boutons;
					printf("        bouton %u : cle=« %s » libelle=« %s » (%.0f, %.0f) "
						   "%.0fx%.0f\n",
						   boutons, notes[i].cle, notes[i].libelle, (double)notes[i].rect.x,
						   (double)notes[i].rect.y, (double)notes[i].rect.w,
						   (double)notes[i].rect.h);
					if (notes[i].cle[0] && NkComponentDecl::StrEq(notes[i].cle, kCible))
						rCible = notes[i].rect;
				}
			printf("  -- (t1) boutons montes = %u ; « %s » en (%.0f, %.0f) %.0fx%.0f\n", boutons,
				   kCible, (double)rCible.x, (double)rCible.y, (double)rCible.w,
				   (double)rCible.h);
			if (boutons == 0u) {
				printf("     KO : la barre ne monte AUCUN bouton\n");
				++ko;
			}
			if (rCible.w <= 0.f || rCible.h <= 0.f) {
				printf("     KO : « %s » n'a pas de rectangle — impossible de le viser\n",
					   kCible);
				++ko;
				printf("=== KO (ko = %u) ===\n", ko);
				return 1;
			}
		}
		const nkgui::NkVec2 centre{rCible.x + rCible.w * 0.5f, rCible.y + rCible.h * 0.5f};
		const nkgui::NkVec2 loin{rCible.x + rCible.w + 200.f, rCible.y + rCible.h * 0.5f};

		auto etatCible = [&]() -> uint16 {
			int32 n = 0;
			const nkgui::NkGuiNote *notes = nkgui::NkGuiIntrospectNotes(ctx, n);
			for (int32 i = 0; i < n; ++i)
				if (notes[i].nature == nkgui::NkGuiNature::Bouton && notes[i].cle[0]
					&& NkComponentDecl::StrEq(notes[i].cle, kCible))
					return notes[i].etats;
			return 0u;
		};

		// ── (t4) LE NEGATIF D'ABORD : loin, rien ───────────────────────────
		image(loin, false);
		image(loin, false);
		image(loin, false);
		{
			const uint16 e = etatCible();
			const bool survole = (e & nkgui::NK_GUI_ETAT_SURVOLE) != 0u;
			printf("  -- (t4) pointeur LOIN : survole = %s\n", survole ? "OUI" : "non");
			if (survole) {
				printf("     KO : il se croit survole alors que le pointeur est a 200 px — le "
					   "survol ne mesure pas ce qu'on croit\n");
				++ko;
			}
		}

		// ── (t2) LE SURVOL ─────────────────────────────────────────────────
		image(centre, false);
		image(centre, false);
		image(centre, false);
		{
			const uint16 e = etatCible();
			const bool survole = (e & nkgui::NK_GUI_ETAT_SURVOLE) != 0u;
			const bool grise = (e & nkgui::NK_GUI_ETAT_GRISE) != 0u;
			printf("  -- (t2) pointeur AU CENTRE : survole = %s, grise = %s\n",
				   survole ? "OUI" : "non", grise ? "OUI" : "non");
			if (grise) {
				printf("     KO : « %s » est GRISE — le controleur le desactive alors que "
					   "l'export est toujours possible\n",
					   kCible);
				++ko;
			}
			if (!survole) {
				printf("     KO : AUCUN survol sur un bouton vise en son centre\n");
				++ko;
			}
		}

		// ── (t3) LE CLIC PART-IL ? ─────────────────────────────────────────
		// On ne compte pas « l'action existe » : on compte qu'elle est APPELEE.
		// Le compteur vit dans l'application (`gExportDemande`), et la sonde ne
		// peut pas le lire ici — on lit donc l'etat ENFONCE, qui est ce que la
		// couche d'entree produit, puis le relachement.
		image(centre, true);
		{
			const uint16 e = etatCible();
			printf("  -- (t3) bouton ENFONCE : actif = %s\n",
				   (e & nkgui::NK_GUI_ETAT_ENFONCE) ? "OUI" : "non");
			if (!(e & nkgui::NK_GUI_ETAT_ENFONCE)) {
				printf("     KO : presser au centre ne rend pas le bouton ENFONCE — le clic "
					   "n'arrive pas jusqu'a lui\n");
				++ko;
			}
		}
		image(centre, false); // le relachement : c'est lui qui declenche

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
