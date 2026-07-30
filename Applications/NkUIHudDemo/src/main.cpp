// =============================================================================
// Applications/NkUIHudDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour Engine/Noge/src/Noge/ECS/Systems/NkUISystem
// (système ECS UI in-game / HUD, cf. Engine/Noge/ROADMAP.md, Phase G2, item
// G2.1, pilier 6). 100 % HEADLESS et CPU-only : le HUD est rendu par le
// backend **Software** de NKCanvas (NkSoftwareFramebuffer, rendu en mémoire
// CPU — AUCUNE fenêtre, AUCUN device GPU, contrainte matérielle assumée :
// extinctions thermiques sur pics GPU), le texte par le rasterizer CPU réel
// de NKFont (police embarquée ProggyClean). Les résultats sont vérifiés PAR
// ASSERTIONS DE PIXELS (NkSoftwareFramebuffer::GetPixel) : le rectangle de
// score rouge est bien aux coordonnées calculées par le layout, la jauge de
// vie est remplie à 50 %, le panel a son fond et sa bordure, le texte a
// réellement déposé des glyphes. Une capture PNG (NKImage) est sauvée pour
// inspection visuelle.
//
// "jenga test" / *_Tests sont bloqués par la politique de workspace (cf.
// Applications/NkEditableMeshDemo, même contournement) : cette application
// console n'est PAS une TestSuite, elle exécute le pipeline réel et affiche
// [OK]/[FAIL] par assertion manuelle.
// =============================================================================
#include "Noge/ECS/Systems/NkUISystem.h"
#include "NKLogger/NkLog.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ecs;

namespace {

	int gFailCount = 0;
	int gPassCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	// Comparaison de pixel avec tolérance ±1 par canal (arrondi float->octet).
	bool PixelIs(NkUISoftwareCanvasBackend &sw, uint32 x, uint32 y, int r, int g, int b) noexcept {
		const math::NkColor c = sw.GetPixel(x, y);
		const int dr = static_cast<int>(c.r) - r;
		const int dg = static_cast<int>(c.g) - g;
		const int db = static_cast<int>(c.b) - b;
		const bool ok = dr >= -1 && dr <= 1 && dg >= -1 && dg <= 1 && db >= -1 && db <= 1;
		if (!ok) {
			logger.Errorf("         pixel(%u,%u) = (%d,%d,%d), attendu (%d,%d,%d)\n", x, y, static_cast<int>(c.r),
						  static_cast<int>(c.g), static_cast<int>(c.b), r, g, b);
		}
		return ok;
	}

} // namespace

int main() {
	logger.Infof("=== NkUIHudDemo — preuve d'execution Noge/ECS/Systems/NkUISystem (HUD headless CPU) ===\n");

	// Fond du HUD : (10, 10, 14) — distinct de toutes les couleurs de widgets.
	constexpr int kBgR = 10, kBgG = 10, kBgB = 14;

	NkWorld world;
	NkUISystem uiSys;
	uiSys.SetClearColor(kBgR, kBgG, kBgB, 255u);

	// Framebuffer CPU 320x180, texte NKFont embarque actif. Aucune fenetre.
	Check(uiSys.Init(320u, 180u, true), "NkUISystem::Init(320, 180) reussi (framebuffer CPU NKCanvas Software)");
	Check(uiSys.TextReady(), "TextReady() : atlas NKFont embarque (ProggyClean) construit");
	NkUISoftwareCanvasBackend *sw = uiSys.Software();
	Check(sw != nullptr, "Software() : backend Software possede par le systeme");
	if (!sw) {
		logger.Errorf("Backend Software indisponible : abandon.\n");
		return 1;
	}

	// -------------------------------------------------------------------
	// Scene HUD : canvas + 6 widgets ECS.
	// -------------------------------------------------------------------

	// Canvas de reference 320x180 -> mapping 1:1 avec le framebuffer.
	const NkEntityId canvasE = world.Create().With<NkCanvas>({}).Build();
	{
		NkCanvas &cv = world.GetRef<NkCanvas>(canvasE);
		cv.referenceWidth = 320.f;
		cv.referenceHeight = 180.f;
	}

	// 1) Rectangle de SCORE rouge : ancre TopLeft, pivot (0,0), pos (10,10),
	//    taille 60x20 -> rect attendu x[10..70), y[10..30).
	const NkEntityId scoreE = world.Create().With<NkRectTransform>({}).With<NkUIImage>({}).Build();
	{
		NkRectTransform &rt = world.GetRef<NkRectTransform>(scoreE);
		rt.anchor = NkAnchors::TopLeft();
		rt.pivot = {0.f, 0.f};
		rt.anchoredPos = {10.f, 10.f};
		rt.sizeDelta = {60.f, 20.f};
		world.GetRef<NkUIImage>(scoreE).color = NkColor4::Red();
	}

	// 2) Jauge de vie : pos (10,40), 100x10, remplie a 50 % (vert sur gris).
	const NkEntityId healthE = world.Create().With<NkRectTransform>({}).With<NkUIProgressBar>({}).Build();
	{
		NkRectTransform &rt = world.GetRef<NkRectTransform>(healthE);
		rt.anchor = NkAnchors::TopLeft();
		rt.pivot = {0.f, 0.f};
		rt.anchoredPos = {10.f, 40.f};
		rt.sizeDelta = {100.f, 10.f};
		NkUIProgressBar &pb = world.GetRef<NkUIProgressBar>(healthE);
		pb.value = 0.5f;
		pb.backgroundColor = NkColor4{0.2f, 0.2f, 0.2f, 1.f};
		pb.fillColor = NkColor4::Green();
		pb.animate = false;
	}

	// 3) Texte "SCORE 42" blanc : pos (10,60), rect 140x20.
	const NkEntityId textE = world.Create().With<NkRectTransform>({}).With<NkUIText>({}).Build();
	{
		NkRectTransform &rt = world.GetRef<NkRectTransform>(textE);
		rt.anchor = NkAnchors::TopLeft();
		rt.pivot = {0.f, 0.f};
		rt.anchoredPos = {10.f, 60.f};
		rt.sizeDelta = {140.f, 20.f};
		NkUIText &txt = world.GetRef<NkUIText>(textE);
		txt.SetText("SCORE 42");
		txt.color = NkColor4::White();
		txt.alignH = NkTextAlign::Left;
		txt.alignV = NkTextAlign::Top;
	}

	// 4) Panel : pos (200,10), 80x40, fond bleu nuit, bordure blanche 2 px.
	const NkEntityId panelE = world.Create().With<NkRectTransform>({}).With<NkUIPanel>({}).Build();
	{
		NkRectTransform &rt = world.GetRef<NkRectTransform>(panelE);
		rt.anchor = NkAnchors::TopLeft();
		rt.pivot = {0.f, 0.f};
		rt.anchoredPos = {200.f, 10.f};
		rt.sizeDelta = {80.f, 40.f};
		NkUIPanel &panel = world.GetRef<NkUIPanel>(panelE);
		panel.backgroundColor = NkColor4{0.1f, 0.1f, 0.3f, 1.f};
		panel.borderColor = NkColor4::White();
		panel.borderWidth = 2.f;
	}

	// 5) Carre jaune ANCRE AU CENTRE (test d'ancrage MiddleCenter/pivot 0.5) :
	//    40x40 centre en (160,90) -> rect attendu x[140..180), y[70..110).
	const NkEntityId centerE = world.Create().With<NkRectTransform>({}).With<NkUIImage>({}).Build();
	{
		NkRectTransform &rt = world.GetRef<NkRectTransform>(centerE);
		rt.anchor = NkAnchors::MiddleCenter();
		rt.pivot = {0.5f, 0.5f};
		rt.anchoredPos = {0.f, 0.f};
		rt.sizeDelta = {40.f, 40.f};
		world.GetRef<NkUIImage>(centerE).color = NkColor4{1.f, 1.f, 0.f, 1.f}; // jaune
	}

	// 6) Image INVISIBLE (magenta, visible=false) : ne doit rien dessiner.
	const NkEntityId hiddenE = world.Create().With<NkRectTransform>({}).With<NkUIImage>({}).Build();
	{
		NkRectTransform &rt = world.GetRef<NkRectTransform>(hiddenE);
		rt.anchor = NkAnchors::TopLeft();
		rt.pivot = {0.f, 0.f};
		rt.anchoredPos = {150.f, 150.f};
		rt.sizeDelta = {20.f, 20.f};
		NkUIImage &img = world.GetRef<NkUIImage>(hiddenE);
		img.color = NkColor4{1.f, 0.f, 1.f, 1.f}; // magenta
		img.visible = false;
	}

	// -------------------------------------------------------------------
	// Une frame HUD : layout + liste de primitives + replay Software.
	// -------------------------------------------------------------------
	uiSys.Execute(world, 1.0f / 60.0f);

	// Layout ecrit dans le composant (role NkUILayoutSystem).
	{
		const NkRectTransform &rt = world.GetRef<NkRectTransform>(scoreE);
		Check(rt.rectX == 10.f && rt.rectY == 10.f && rt.rectW == 60.f && rt.rectH == 20.f,
			  "layout : rect calcule du score ecrit dans NkRectTransform (10,10,60,20)");
	}

	// Liste de primitives abstraite (decouplage systeme / backend de dessin).
	{
		const NkVector<NkUIDrawCmd> &cmds = uiSys.Commands();
		nk_usize rects = 0, texts = 0;
		for (nk_usize i = 0; i < cmds.Size(); ++i) {
			if (cmds[i].kind == NkUIDrawCmd::Kind::Rect)
				++rects;
			else
				++texts;
		}
		// Attendu : score 1 + jauge 2 (fond+fill) + panel 5 (fond+4 bandes de
		// bordure) + carre jaune 1 = 9 rects ; 1 texte ("SCORE 42").
		Check(rects == 9u, "liste de primitives : 9 commandes Rect (score 1, jauge 2, panel 5, centre 1)");
		Check(texts == 1u, "liste de primitives : 1 commande Text");
	}

	// ── Rectangle de score ROUGE aux coordonnees attendues ──────────────
	Check(PixelIs(*sw, 40u, 20u, 255, 0, 0), "pixel(40,20) : interieur du rectangle de score = ROUGE (255,0,0)");
	Check(PixelIs(*sw, 10u, 10u, 255, 0, 0), "pixel(10,10) : coin haut-gauche du score = ROUGE");
	Check(PixelIs(*sw, 69u, 29u, 255, 0, 0), "pixel(69,29) : coin bas-droit (inclus) du score = ROUGE");
	Check(PixelIs(*sw, 5u, 5u, kBgR, kBgG, kBgB), "pixel(5,5) : hors du score = fond");
	Check(PixelIs(*sw, 75u, 20u, kBgR, kBgG, kBgB), "pixel(75,20) : a droite du score = fond");

	// ── Jauge de vie 50 % : x[10..110), remplissage vert x[10..60) ──────
	Check(PixelIs(*sw, 15u, 45u, 0, 255, 0), "pixel(15,45) : jauge remplie a gauche = VERT");
	Check(PixelIs(*sw, 58u, 45u, 0, 255, 0), "pixel(58,45) : juste avant 50%% = VERT");
	Check(PixelIs(*sw, 62u, 45u, 51, 51, 51), "pixel(62,45) : juste apres 50%% = fond de jauge gris");
	Check(PixelIs(*sw, 105u, 45u, 51, 51, 51), "pixel(105,45) : fin de jauge = fond de jauge gris");
	Check(PixelIs(*sw, 115u, 45u, kBgR, kBgG, kBgB), "pixel(115,45) : a droite de la jauge = fond");

	// ── Panel : fond + bordure blanche 2 px ─────────────────────────────
	Check(PixelIs(*sw, 240u, 30u, 26, 26, 77), "pixel(240,30) : interieur du panel = fond bleu nuit");
	Check(PixelIs(*sw, 240u, 11u, 255, 255, 255), "pixel(240,11) : bordure haute du panel = BLANC");
	Check(PixelIs(*sw, 201u, 30u, 255, 255, 255), "pixel(201,30) : bordure gauche du panel = BLANC");

	// ── Ancrage centre : carre jaune centre en (160,90) ─────────────────
	Check(PixelIs(*sw, 160u, 90u, 255, 255, 0), "pixel(160,90) : centre du carre ancre MiddleCenter = JAUNE");
	Check(PixelIs(*sw, 141u, 71u, 255, 255, 0), "pixel(141,71) : coin du carre jaune (rect [140..180)x[70..110))");
	Check(PixelIs(*sw, 135u, 90u, kBgR, kBgG, kBgB), "pixel(135,90) : hors du carre jaune = fond");

	// ── Image invisible : aucun pixel magenta ───────────────────────────
	Check(PixelIs(*sw, 160u, 160u, kBgR, kBgG, kBgB), "pixel(160,160) : image visible=false non dessinee");

	// ── Texte : glyphes reellement deposes dans le rect [10..150)x[60..80) ──
	{
		int whitePixels = 0, inkPixels = 0;
		for (uint32 y = 60u; y < 80u; ++y) {
			for (uint32 x = 10u; x < 150u; ++x) {
				const math::NkColor c = sw->GetPixel(x, y);
				if (c.r == 255u && c.g == 255u && c.b == 255u)
					++whitePixels;
				if (c.r != kBgR || c.g != kBgG || c.b != kBgB)
					++inkPixels;
			}
		}
		logger.Infof("         texte : %d pixels blancs purs, %d pixels encres dans le rect\n", whitePixels,
					 inkPixels);
		Check(whitePixels >= 20, "texte 'SCORE 42' : >= 20 pixels blancs purs (glyphes ProggyClean blittes)");
		Check(inkPixels >= 40, "texte 'SCORE 42' : >= 40 pixels encres au total dans le rect");
	}

	// ── Canvas.visible = false -> HUD entier masque, puis retabli ───────
	world.GetRef<NkCanvas>(canvasE).visible = false;
	uiSys.Execute(world, 1.0f / 60.0f);
	Check(PixelIs(*sw, 40u, 20u, kBgR, kBgG, kBgB), "canvas.visible=false : le score n'est plus dessine (fond)");

	world.GetRef<NkCanvas>(canvasE).visible = true;
	uiSys.Execute(world, 1.0f / 60.0f);
	Check(PixelIs(*sw, 40u, 20u, 255, 0, 0), "canvas.visible=true retabli : le score est redessine");

	// ── Capture PNG (NKImage) pour inspection visuelle ──────────────────
	const char *pngPath = "NkUIHudDemo_hud.png";
	const bool saved = sw->SavePNG(pngPath);
	Check(saved, "SavePNG('NkUIHudDemo_hud.png') : capture du framebuffer via NKImage");
	if (saved)
		logger.Infof("         capture : %s (dans le repertoire courant)\n", pngPath);

	uiSys.Shutdown();
	Check(!uiSys.IsReady(), "IsReady() == false apres Shutdown()");

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
