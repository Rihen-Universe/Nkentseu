#pragma once
// -----------------------------------------------------------------------------
// @File    CasHoteDansMenu.h
// @Brief   (b20) Une zone `Host` dans un MENU ne doit RIEN réserver.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE ÇA MESURE
// =============================================================================
//  Rodolf, 28/09 : « plusieurs menus ou sous-menus sont mal alignés ». Sur sa
//  capture, le sous-menu « Thème » montrait un grand vide, puis « Sombre » et
//  « Clair » tout en bas.
//
//  🔴 LA CAUSE ÉTAIT UNE RÉSERVATION. Le rôle `Host` appelait
//     `NextItemRect(-1, ItemHeight() * 4)` AVANT de passer la main à l'hôte —
//     une surface, ce qui est juste dans un panneau. Mais un hôte de MENU
//     dessine au FIL (`MenuItem` prend `NextItemRect`) : on obtenait quatre
//     rangées vides, puis les entrées en dessous.
//
//   (h1) l'hôte est bien APPELÉ, et il dessine.
//   (h2) 🔴 SON ENTRÉE COMMENCE LÀ OÙ LE MENU EN ÉTAIT — pas quatre rangées
//        plus bas. C'est le critère, et il est géométrique : un compteur
//        d'entrées aurait été vert avec le trou.
//   (h3) NÉGATIF : hors d'un menu, la réservation RESTE. Un `Host` de panneau
//        EST une surface ; la supprimer partout aurait cassé le contrat que les
//        panneaux tiennent — et rien dans (h1)/(h2) ne l'aurait dit.
// -----------------------------------------------------------------------------

/// Comparaison de mots, ecrite ici : ce banc n'inclut pas NKUIDesign, donc pas
/// `NkComponentDecl::StrEq`. Quatre lignes valent mieux qu'une dependance vers
/// une application depuis un banc du noyau.
static bool MemeMot(const char *a, const char *b) {
	if (!a || !b)
		return false;
	while (*a && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

static void CasHoteDansMenu() {
	// =====================================================================
	printf("\n-- (b20) une zone `Host` dans un MENU ne reserve rien\n");
	// =====================================================================

	/// L'hôte d'épreuve : il dessine UNE entrée, et retient le rectangle qu'on
	/// lui a donné ainsi que celui de ce qu'il a dessiné.
	struct HoteEpreuve : public NkGuiMonteHooks {
			uint32 appels = 0u;
			NkRect zoneRecue{0.f, 0.f, 0.f, 0.f};
			NkRect itemDessine{0.f, 0.f, 0.f, 0.f};
			bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &zone) noexcept override {
				(void)nom;
				++appels;
				zoneRecue = zone;
				(void)MenuItem(ctx, "Entrée de l'hôte", nullptr, true, false);
				itemDessine = ctx.layout.prevItem;
				return true;
			}
	};

	// ── (h1)(h2) DANS UN MENU ──────────────────────────────────────────
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n"
								   "    Menu \"m\" {\n"
								   "      label = \"Épreuve\"\n"
								   "      MenuItem \"avant\" { label = \"Avant\" }\n"
								   "      Host \"ma.zone\" { hint: \"l'hote\" }\n"
								   "    }\n"
								   "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 400, 300),
			  "(h1) le document a `Host` dans un menu se charge");
		NkGuiIntrospectActiver(s.ctx, true);
		HoteEpreuve h;
		// ⚠️ PLUSIEURS IMAGES ET UN CLIC : un menu se MESURE une image et
		//    s'applique a la suivante. Une seule image ne verrait jamais le
		//    contenu du menu, et l'hote ne serait jamais appele.
		for (uint32 img = 0u; img < 4u; ++img) {
			s.ctx.input.mousePos = {30.f, 12.f};
			s.ctx.input.mouseDown[0] = (img == 1u);
			s.Image(&h);
		}
		// Le rectangle de l'entree « Avant », lu dans le releve : c'est la
		// reference. L'entree de l'hote doit venir JUSTE apres.
		NkRect avant{0.f, 0.f, 0.f, 0.f};
		{
			int32 n = 0;
			const NkGuiNote *notes = NkGuiIntrospectNotes(s.ctx, n);
			for (int32 i = 0; i < n; ++i)
				if (notes[i].nature == NkGuiNature::EntreeMenu && notes[i].cle[0]
					&& MemeMot(notes[i].cle, "avant"))
					avant = notes[i].rect;
		}
		printf("        appels = %u ; « Avant » y=%.0f h=%.0f ; entree de l'hote y=%.0f ; zone "
			   "recue h=%.0f\n",
			   h.appels, (double)avant.y, (double)avant.h, (double)h.itemDessine.y,
			   (double)h.zoneRecue.h);
		Check(h.appels > 0u, "(h1) l'hote EST appele dans le menu");
		Check(h.itemDessine.h > 0.f, "(h1) et il a dessine quelque chose");
		if (avant.h > 0.f && h.itemDessine.h > 0.f) {
			// Une rangee d'ecart tolere l'espacement ; quatre, non.
			const float32 ecart = h.itemDessine.y - (avant.y + avant.h);
			printf("        ecart entre « Avant » et l'entree de l'hote = %.1f px (une rangee "
				   "vaut %.0f)\n",
				   (double)ecart, (double)avant.h);
			Check(ecart < avant.h,
				  "(h2) L'ENTREE DE L'HOTE SUIT LA PRECEDENTE — pas quatre rangees plus bas");
		}
		// ⚠️ ET LA ZONE RECUE EST DE HAUTEUR NULLE DANS UN MENU : c'est ce qui
		//    dit qu'on n'a rien reserve. Une zone de 4 rangees ici signifierait
		//    que le trou est revenu, meme si l'ecart mesure etait bon par hasard.
		Check(h.zoneRecue.h <= 0.5f,
			  "(h2) la zone rendue a l'hote ne RESERVE rien (hauteur nulle)");
		s.exe.Debrancher(s.ctx);
	}

	// ── (h3) LE NEGATIF : HORS MENU, LA RESERVATION RESTE ──────────────
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  Host \"ma.zone\" { hint: \"l'hote\" }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 400, 300),
			  "(h3) le meme `Host`, hors de tout menu");
		HoteEpreuve h;
		s.Image(&h);
		printf("        [hors menu] appels = %u, zone recue %.0fx%.0f\n", h.appels,
			   (double)h.zoneRecue.w, (double)h.zoneRecue.h);
		Check(h.appels > 0u, "(h3) l'hote est appele hors menu aussi");
		Check(h.zoneRecue.h > 1.f,
			  "(h3) ET LA ZONE RESERVE TOUJOURS — un `Host` de panneau EST une surface");
		s.exe.Debrancher(s.ctx);
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
