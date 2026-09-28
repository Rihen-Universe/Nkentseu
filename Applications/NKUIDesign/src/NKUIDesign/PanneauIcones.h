#pragma once
// -----------------------------------------------------------------------------
// @File    PanneauIcones.h
// @Brief   LA BIBLIOTHÈQUE D'ICÔNES — ce que le jeu posé contient, visible et
//          copiable.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE PANNEAU EXISTE
// =============================================================================
//  Rodolf, 28/09, après « j'écris le jeu d'icônes dans le kit, pour que toutes
//  les applications en profitent » : « aussi dans nkuidesign pour les
//  bibliothèques d'icônes ».
//
//  Un jeu d'icônes qu'on ne peut pas VOIR n'est utilisable que par celui qui l'a
//  écrit : pour poser `icon = "degrouper"` dans un document, il faut savoir que
//  ce nom existe et à quoi il ressemble. Sans ce panneau, la seule source est le
//  code source du kit — c'est-à-dire, pour un utilisateur, rien.
//
// =============================================================================
//  ⚠️ IL NE POSE PAS L'ICÔNE SUR LA SÉLECTION, ET C'EST ÉCRIT PLUTÔT QUE TU
//     T'EN APERÇOIVES À L'USAGE
// =============================================================================
//  `NkUINode` n'a AUCUN champ `icon` : la clé voyage dans l'archive `.nkgui` et
//  le modèle d'édition ne sait pas la tenir (le lecteur la compte dans
//  `attributsNonPortes`, avec `tooltip`, `shortcut` et `enabled`). Un bouton
//  « appliquer à la sélection » écrirait donc dans le vide, et la perte serait
//  silencieuse.
//
//  Ce panneau fait donc ce qu'il peut faire HONNÊTEMENT : il montre, il nomme,
//  et il MET LA LIGNE DANS LE PRESSE-PAPIERS. Le jour où `NkUINode` porte les
//  attributs d'interface, le clic posera l'icône.
//  **CONDITION DE RETRAIT DE CETTE NOTE : ce jour-là.**
//
// ⚠️ IL LIT LE JEU RÉELLEMENT POSÉ (`NkGuiIconesPosees`), jamais une liste
//    écrite ici. Deux listes ne peuvent pas se contredire — donc ne prouvent
//    rien — et celle-ci deviendrait fausse au premier glyphe ajouté au kit.
// -----------------------------------------------------------------------------

namespace nkuidesign {

	class PanneauIcones : public nkentseu::editorkit::NkEditorPanel {
		public:
			explicit PanneauIcones(DesignState &st) noexcept
				: nkentseu::editorkit::NkEditorPanel("nkuidesign_panneau_icones", "Icônes",
													 nkentseu::editorkit::NkEditorDockSide::NK_LEFT),
				  mSt(st) {
			}

			void OnUI(NkEditorFrameContext &ec) override {
				using namespace nkentseu;
				auto &ctx = ec.Ui();
				const nkgui::NkGuiIconSet *jeu = nkgui::NkGuiIconesPosees();
				if (!jeu || jeu->Count() <= 0) {
					// 🔴 LE CAS VIDE SE DIT, IL NE SE TAIT PAS. Un panneau
					//    vierge se lit comme « il n'y a pas d'icônes » ; la
					//    vraie information est « personne n'en a posé », et
					//    elle nomme le coupable.
					nkgui::Text(ctx, "Aucun jeu d'icônes posé — `NkGuiPoserIcones` n'a "
									 "jamais été appelé dans cette application.");
					return;
				}
				nkgui::Text(ctx, "Cliquer une icône copie sa ligne `.nkgui`.");
				nkgui::Separator(ctx);

				const nkgui::NkRect zone = ctx.layout.region;
				const float32 cote = 34.f, pas = cote + 6.f;
				int32 parLigne = (int32)((zone.w - 12.f) / pas);
				if (parLigne < 1)
					parLigne = 1;
				const float32 x0 = zone.x + 8.f;
				float32 y = ctx.layout.cursor.y + 4.f;
				int32 col = 0;
				for (int32 i = 0; i < jeu->Count(); ++i) {
					const nkgui::NkGuiIconGlyph *g = jeu->GlyphAt(i);
					if (!g || !g->name[0])
						continue;
					// ⚠️ LE GLYPHE DE SECOURS N'EST PAS UNE ICÔNE DE LA
					//    BIBLIOTHÈQUE : c'est le dessin qui dit « nom inconnu ».
					//    L'offrir au choix ferait écrire `icon = "__inconnu__"`
					//    dans un document, ce qui n'a aucun sens.
					if (g->name[0] == '_' && g->name[1] == '_')
						continue;
					const nkgui::NkRect r{x0 + (float32)col * pas, y, cote, cote};
					const bool survol = nkgui::NkGuiRectContains(r, ctx.input.mousePos);
					ctx.DL().AddRectFilled(r, survol ? ctx.theme.buttonHover : ctx.theme.button,
										   4.f);
					const float32 m = cote * 0.22f;
					(void)nkgui::AddIcon(ctx.DL(), *jeu, jeu->Find(g->name),
										 {r.x + m, r.y + m, cote - 2.f * m, cote - 2.f * m},
										 ctx.theme.text);
					if (survol) {
						nkentseu::editorkit::NkTooltip(ctx, true, g->name);
						if (ctx.input.mouseClicked[0]) {
							char ligne[128];
							nkentseu::NkSnprintf(ligne, sizeof(ligne), "icon = \"%s\"", g->name);
							ctx.SetClipboard(ligne);
							char msg[192];
							nkentseu::NkSnprintf(msg, sizeof(msg),
												 "Copié : %s — à coller sur un Button du .nkgui.",
												 ligne);
							mSt.DireAuPied(msg);
						}
					}
					if (++col >= parLigne) {
						col = 0;
						y += pas;
					}
				}
				// Le curseur du flux reprend SOUS la grille : sans ça, ce qu'un
				// autre widget dessinerait ensuite se poserait par-dessus.
				ctx.layout.cursor.y = y + (col ? pas : 0.f) + 6.f;
			}

		private:
			DesignState &mSt;
	};

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
