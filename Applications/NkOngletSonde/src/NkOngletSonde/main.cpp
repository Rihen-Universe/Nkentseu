// =============================================================================
// @File    main.cpp — NkOngletSonde
// @Brief   SONDE DE MESURE du canal `onglets.questions.md` : (o1) la bande
//          d'onglets partagee du kit, (o2) les accents.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
//
// ⚠️ CETTE APPLICATION N'EST PAS UN PRODUIT. Sa fenetre le dit dans son titre,
//    et elle se ferme d'elle-meme. Une fenetre d'agent s'est deja fait
//    photographier a la place du produit : le titre est le seul endroit qu'on
//    regarde avant de croire une capture.
//
// ⚠️ AUCUNE INJECTION D'ENTREE — ni souris, ni clavier. La sonde DESSINE et
//    MESURE ; elle ne pilote rien et ne photographie pas l'ecran, seulement son
//    propre backbuffer.
//
// =============================================================================
//  (o2) LES ACCENTS — LE NEGATIF EST DANS LA MEME IMAGE QUE LA MESURE
// =============================================================================
//  La question du canal : « pourquoi les libelles du depot n'ont-ils pas
//  d'accents ? » Trois causes possibles a departager PAR MESURE :
//    (1) la police du kit ne porte pas les glyphes accentues ;
//    (2) l'encodage source n'arrive pas intact jusqu'au rendu ;
//    (3) c'est un choix ancien que personne n'a rouvert.
//
//  ⚠️ LE PIEGE D'UN BANC QUI NE SAIT DIRE QUE « OUI ». Afficher un « é », voir
//     de l'encre, et conclure « la police les a » — c'est exactement le genre de
//     temoin vert qui ne prouve rien, parce qu'on n'a pas montre qu'il SAIT
//     dire non. Ce depot a paye trois fois cette famille de defaut (« sonde qui
//     ne temoigne pas », « un negatif incapable de refuter »).
//
//  DONC la meme image porte des glyphes qui DOIVENT etre absents, et on sait
//  pourquoi : la plage par defaut de l'atlas est `{0x0020, 0x00FF}`
//  (`NkFontAtlas.cpp:141`, `sRangesDefault`). Tout ce qui est au-dela n'est
//  rasterise que si l'application a pose une police de REPLI externe
//  (`NkSetFallbackFontPaths`) — ce que seul NKCode fait. La sonde n'en pose
//  aucune.
//    DANS la plage, donc attendu PRESENT  : é è ê à ç ù « »  (U+00E9...U+00BB)
//    HORS la plage, donc attendu ABSENT   : ─ (U+2500) et 中 (U+4E2D)
//
//  Si les deux familles rendent de l'encre, mon raisonnement sur la plage est
//  faux et la mesure est a jeter. Si aucune n'en rend, c'est le dessin ou la
//  capture qui est mort, pas la police. Seul le contraste entre les deux
//  conclut.
//
// =============================================================================
//  (o1) LA BANDE D'ONGLETS — CE QUE LA SONDE EN MESURE
// =============================================================================
//  Elle pose le composant PARTAGE `tab_strip` sur la coquille et imprime la
//  geometrie que celui-ci RAPPORTE (`NkTabStripResult::tabs`) : chaque onglet,
//  son x, sa largeur, son rectangle de libelle. Ces nombres viennent du MEME
//  code que NK3DModeler appelle par son propre peintre.
//
//  ⚠️ LA MUTATION SE PILOTE DE L'EXTERIEUR (`NKSONDE_MUT_PADX`) et elle passe
//     par une `NkComponentInstance`, c'est-a-dire par la porte NORMALE des
//     reglages — pas par une recompilation du kit. C'est ce qui permet de
//     montrer, dans la meme construction, que la geometrie des onglets obeit a
//     la metrique partagee.
//
// REGLAGES (variables d'environnement : on ne construit qu'une fois)
//   NKSONDE_OUT=<chemin>    fichier de capture (.png)
//   NKSONDE_FRAME=<n>       image capturee (defaut 6) ; fermeture a n+3
//   NKSONDE_MUT_PADX=<px>   ecrase `tab_pad_x` ; 0 = le defaut de la declaration
//   NKSONDE_ACCENTS=0|1     1 = la ligne d'accents est dessinee (defaut 1)
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKEditorKit/Components/NkTabStripModel.h"
#include "NKMemory/NkUniquePtr.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkOngletSonde";
	d.appVersion = "0.1.0";
	return d;
})());

namespace {

	int32 EnvI(const char *nom, int32 defaut) {
		const char *v = std::getenv(nom);
		return (v && *v) ? (int32)std::atoi(v) : defaut;
	}
	const char *EnvS(const char *nom, const char *defaut) {
		const char *v = std::getenv(nom);
		return (v && *v) ? v : defaut;
	}

	// ── LES GLYPHES MESURES ──────────────────────────────────────────────────
	// `attendu` : 1 = doit apparaitre (dans la plage par defaut de l'atlas),
	//             0 = doit rester absent (hors plage, aucun repli pose).
	struct Glyphe {
			const char *utf8;
			nkft_uint32 cp;
			const char *nom;
			int32 attendu;
	};
	const Glyphe kGlyphes[] = {
		// DANS la plage {0x0020, 0x00FF} — le francais de Rodolf
		{"e", 0x0065u, "e (temoin ASCII)", 1},
		{"\xC3\xA9", 0x00E9u, "e accent aigu", 1},
		{"\xC3\xA8", 0x00E8u, "e accent grave", 1},
		{"\xC3\xAA", 0x00EAu, "e accent circonflexe", 1},
		{"\xC3\xA0", 0x00E0u, "a accent grave", 1},
		{"\xC3\xA7", 0x00E7u, "c cedille", 1},
		{"\xC3\xB9", 0x00F9u, "u accent grave", 1},
		{"\xC2\xAB", 0x00ABu, "guillemet ouvrant", 1},
		{"\xC2\xBB", 0x00BBu, "guillemet fermant", 1},
		// HORS de la plage — LE NEGATIF. Aucune police de repli n'est posee.
		{"\xE2\x94\x80", 0x2500u, "trait horizontal (HORS PLAGE)", 0},
		{"\xE4\xB8\xAD", 0x4E2Du, "ideogramme (HORS PLAGE)", 0},
	};
	const int32 kNGlyphes = (int32)(sizeof(kGlyphes) / sizeof(kGlyphes[0]));

	void MenuBarVide(NkEditorFrameContext &, void *) {
	}

	// ── LE PANNEAU : il dessine la ligne de glyphes et cadence la capture ────
	class PanneauSonde final : public NkEditorPanel {
		public:
			PanneauSonde() : NkEditorPanel("Sonde", NkEditorDockSide::NK_CENTER) {
			}

			NkEditorShell *shell = nullptr;
			int32 frame = 0;
			int32 frameCapture = 6;
			const char *sortie = "onglets.png";
			bool armee = false;
			bool accents = true;
			// Les rectangles ou chaque glyphe a ete dessine — imprimes pour que le
			// compteur de pixels sache OU regarder, au lieu de le deviner.
			float32 boiteX[16] = {}, boiteY[16] = {}, boiteW[16] = {}, boiteH[16] = {};

			void OnUI(NkEditorFrameContext &ec) override {
				++frame;
				if (!shell)
					return;
				nkgui::NkGuiContext &ui = shell->Ui();

				if (accents)
					DessineLesGlyphes(ui);

				if (frame == frameCapture && !armee) {
					// ⚠️ LA CONDITION D'ESSAI SE MESURE, ELLE NE SE SUPPOSE PAS.
					std::printf("[SONDE] etat a la capture : scale=%.4f  ItemHeight=%.2f  "
								"titleBarH=%.2f  viewW=%d viewH=%d  police=%s\n",
								ui.scale, ui.ItemHeight(), ui.titleBarH, ui.viewW, ui.viewH,
								(ui.font && ui.font->Valid()) ? "CHARGEE" : "ABSENTE");
					ImprimeLaCouverture(ui);
					ImprimeLesOnglets();
					ImprimeLesBoites();
					NkIEditorRenderer *r = shell->Renderer();
					// ⚠️ ON N'AFFIRME PAS QUE LA CAPTURE A EU LIEU : on journalise le
					//    retour de `CaptureNext`, qui rend faux par defaut.
					const bool ok = r ? r->CaptureNext(sortie) : false;
					armee = ok;
					std::printf("[SONDE] CaptureNext(\"%s\") -> %s\n", sortie,
								ok ? "ARMEE" : "REFUSEE");
					std::fflush(stdout);
				}
				if (frame >= frameCapture + 3)
					shell->RequestClose();
				(void)ec;
			}

		private:
			// ── LE DESSIN DES GLYPHES ────────────────────────────────────────
			// Chacun dans SA boite, sur un fond NOIR PUR pose par la sonde. Le
			// compteur teste ensuite « y a-t-il un pixel non noir dans la boite ? ».
			// Fond franc + boites disjointes = aucun glyphe ne peut etre credite de
			// l'encre de son voisin.
			void DessineLesGlyphes(nkgui::NkGuiContext &ui) {
				if (!ui.font || !ui.font->Valid())
					return;
				auto &dl = ui.DL();
				const float32 lh = ui.font->LineHeight();
				const float32 asc = ui.font->Ascent();
				const float32 bw = 46.f, bh = lh + 10.f;
				const float32 x0 = 40.f, y0 = 220.f;
				for (int32 i = 0; i < kNGlyphes && i < 16; ++i) {
					const float32 bx = x0 + (float32)i * (bw + 6.f);
					const nkgui::NkRect boite{bx, y0, bw, bh};
					// FOND NOIR PUR (0,0,0) : il n'existe nulle part ailleurs dans
					// l'image (le theme sombre du kit part de #212121).
					dl.AddRectFilled(boite, nkgui::NkColor{0, 0, 0, 255});
					// GLYPHE EN BLANC PUR (255,255,255).
					dl.AddText(ui.font->Face(), ui.font->TexId(),
							   {bx + bw * 0.5f - 6.f, y0 + (bh - lh) * 0.5f + asc},
							   kGlyphes[i].utf8, nkgui::NkColor{255, 255, 255, 255});
					boiteX[i] = bx;
					boiteY[i] = y0;
					boiteW[i] = bw;
					boiteH[i] = bh;
				}
			}

			// ── LA COUVERTURE, DEMANDEE A LA POLICE ELLE-MEME ────────────────
			// ⚠️ `FindGlyphNoFallback` ET `FindGlyph` : le premier dit si CETTE
			//    police porte le glyphe, le second si quelque chose finit par le
			//    rendre (repli compris). Les confondre, c'est prendre un repli pour
			//    une couverture.
			void ImprimeLaCouverture(nkgui::NkGuiContext &ui) {
				if (!ui.font || !ui.font->Valid()) {
					std::printf("[SONDE] AUCUNE POLICE : la mesure d'accents est NULLE\n");
					return;
				}
				const NkFont *f = ui.font->Face();
				for (int32 i = 0; i < kNGlyphes; ++i) {
					const NkFontGlyph *g = f->FindGlyphNoFallback(kGlyphes[i].cp);
					const NkFontGlyph *gf = f->FindGlyph(kGlyphes[i].cp);
					std::printf("[GLYPHE] U+%04X %-32s attendu=%d  propre=%s  avec_repli=%s  "
								"largeur=%.2f\n",
								(unsigned)kGlyphes[i].cp, kGlyphes[i].nom, kGlyphes[i].attendu,
								g ? "OUI" : "non", gf ? "OUI" : "non",
								ui.font->MeasureWidth(kGlyphes[i].utf8));
				}
			}

			void ImprimeLesBoites() {
				for (int32 i = 0; i < kNGlyphes && i < 16; ++i)
					std::printf("[BOITE] %d  U+%04X  attendu=%d  x=%.1f y=%.1f w=%.1f h=%.1f\n", i,
								(unsigned)kGlyphes[i].cp, kGlyphes[i].attendu, boiteX[i], boiteY[i],
								boiteW[i], boiteH[i]);
			}

			// ── LA GEOMETRIE QUE LE COMPOSANT PARTAGE A RAPPORTEE ────────────
			void ImprimeLesOnglets() {
				const NkTabStripResult &r = shell->TabStripResult();
				std::printf("[ONGLETS] n=%d  usedW=%.2f  debordement=%d\n", (int)r.tabs.Size(),
							r.usedW, r.overflow ? 1 : 0);
				for (usize i = 0; i < r.tabs.Size(); ++i)
					std::printf("[ONGLET] %u  id=%u  x=%.2f y=%.2f w=%.2f h=%.2f  "
								"libelleX=%.2f libelleW=%.2f\n",
								(unsigned)i, (unsigned)r.tabs[i].id, r.tabs[i].x, r.tabs[i].y,
								r.tabs[i].w, r.tabs[i].h, r.tabs[i].labelX, r.tabs[i].labelW);
			}
	};

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	const char *sortie = EnvS("NKSONDE_OUT", "onglets.png");
	const int32 frameCap = EnvI("NKSONDE_FRAME", 6);
	const int32 mutPadX = EnvI("NKSONDE_MUT_PADX", 0);
	const int32 accents = EnvI("NKSONDE_ACCENTS", 1);

	std::printf("[SONDE] out=%s frame=%d mut_tab_pad_x=%d accents=%d\n", sortie, frameCap, mutPadX,
				accents);
	std::fflush(stdout);

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	static NkEditorCanvasRenderer canvasRenderer;

	NkEditorShellConfig cfg;
	// ⚠️ LE TITRE EST UNE GARDE, PAS UNE DECORATION.
	cfg.title = "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***";
	cfg.width = 1280;
	cfg.height = 720;
	// ⚠️ DX11 N'EST PAS UN GOUT : `NkRenderWindow::Capture` ne relit le
	//    backbuffer que sur DX11 (mesure du canal chrome : en OpenGL la capture
	//    s'arme et n'ecrit rien, six fichiers absents).
	cfg.graphicsApi = NkEditorGfxApi::DX11;
	cfg.renderer = &canvasRenderer;
	if (!shell || !shell->Init(cfg)) {
		std::fprintf(stderr, "[SONDE] Init du shell ECHOUE\n");
		return -1;
	}

	// ── (o1) LA BANDE D'ONGLETS PARTAGEE ────────────────────────────────────
	// ⚠️ LES LIBELLES PORTENT DES ACCENTS, ET C'EST LA MOITIE DE LA MESURE (o2) :
	//    ils traversent la MEME chaine que les libelles du produit — source UTF-8,
	//    `NkString`, `NkComponentPaint::Text`, `NkGuiDrawList::AddText`. Un « é »
	//    dessine dans une boite d'essai prouve la police ; un « é » dans un ONGLET
	//    prouve la chaine entiere.
	static NkTabStripModel modele;
	{
		NkTabItem a;
		a.id = 1;
		a.label = NkString("Sc\xC3\xA8ne principale"); // « Scène principale »
		a.modified = true;
		modele.tabs.PushBack(a);
		NkTabItem b;
		b.id = 2;
		b.label = NkString("\xC3\x89" "clairage"); // « Éclairage » (majuscule accentuee)
		modele.tabs.PushBack(b);
		NkTabItem c;
		c.id = 3;
		c.label = NkString("For\xC3\xAAt \xE2\x80\x94 \xC3\xA9t\xC3\xA9"); // « Forêt — été »
		modele.tabs.PushBack(c);
		NkTabItem d;
		d.id = 4;
		d.label = NkString("Scene sans accent"); // le TEMOIN ASCII
		modele.tabs.PushBack(d);
		modele.active = 1;
	}
	shell->SetTabStrip(&modele);

	// ── LA MUTATION, PAR LA PORTE NORMALE DES REGLAGES ──────────────────────
	// Une `NkComponentInstance` liee a la declaration du composant : c'est ce
	// que l'editeur NkUIDesign poserait. Muter ici prouve que la GEOMETRIE des
	// onglets obeit a la metrique PARTAGEE — la meme que lit NK3DModeler.
	static NkComponentInstance inst;
	if (mutPadX > 0) {
		inst.Bind(NkTabStripDecl());
		inst.SetMetric("tab_pad_x", (float32)mutPadX);
		NkTabStripStyle s;
		s.bandBg = (uint16)NkRole::PanelBg;
		s.border = (uint16)NkRole::Border;
		s.tabBg = (uint16)NkRole::InputBg;
		s.tabHoverBg = (uint16)NkRole::PanelBg;
		s.tabActiveBg = (uint16)NkRole::PanelHeader;
		s.text = (uint16)NkRole::Text;
		s.textMuted = (uint16)NkRole::TextMuted;
		s.accent = (uint16)NkRole::AccentUi;
		s.values = &inst;
		shell->SetTabStripStyle(s);
	}

	shell->ApplyTheme(NkTheme::Dark());
	shell->SetMenuBar(&MenuBarVide, nullptr);
	shell->SetActivityBars(false, false);
	shell->SetFooterZoomIndicator(false);
	// (o3) : 28 px, la cote du modeleur. Sans cet appel la coquille rend 22.
	shell->SetStatusBarHeight(28.f);

	static PanneauSonde panneau;
	panneau.shell = shell.Get();
	panneau.frameCapture = frameCap;
	panneau.sortie = sortie;
	panneau.accents = (accents != 0);
	shell->AddPanel(&panneau);

	const int rc = shell->Run();

	// ⚠️ « ARMEE » N'EST PAS « ECRITE » : on verifie le FICHIER, seule preuve qui
	//    ne depende d'aucune promesse.
	long taille = -1;
	if (FILE *f = std::fopen(sortie, "rb")) {
		std::fseek(f, 0, SEEK_END);
		taille = std::ftell(f);
		std::fclose(f);
	}
	std::printf("[SONDE] fin, code=%d, images=%d, capture=%s (%ld octets)\n", rc, panneau.frame,
				taille >= 0 ? "ECRITE" : "ABSENTE", taille);
	std::fflush(stdout);
	if (taille < 0)
		std::fprintf(stderr, "[SONDE] ECHEC : aucun fichier de capture a '%s'\n", sortie);
	return rc;
}
