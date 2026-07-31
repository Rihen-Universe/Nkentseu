// =============================================================================
// main.cpp — Point d'entree de NK3DModeler.
//
// L'INTERFACE EST PEINTE DIRECTEMENT, sans passer par NkEditorShell.
//   Le shell de NKEditorKit apporte sa PROPRE chrome — barre de menus, barre
//   d'etat, docking, palette — pensee pour un IDE. Elle est excellente pour
//   NKCode et elle empeche de coller a une maquette au pixel pres : on passerait
//   son temps a lutter contre une disposition qu'on ne controle pas.
//   NK3DModeler doit ressembler EXACTEMENT a l'ecran A valide par Rihen, donc on
//   ouvre la fenetre, on prend la draw list, et on peint.
//
// CE QUI EST DEJA VRAI ET N'EST PAS DE LA MAQUETTE
//   * pas une seule couleur en dur dans le rendu : tout passe par les roles du
//     theme, y compris les axes, les types d'assets et les six roles propres au
//     produit. C'est ce qui fera fonctionner le theme clair sans y retoucher ;
//   * les raccourcis affiches sont LUS dans NkShortcutTable, jamais recopies :
//     rebinder une touche changera l'affichage tout seul.
//
// CE QUI RESTE A FAIRE, par iterations successives comme convenu : les
//   interactions (survol, clic, redimensionnement des zones), puis la vue 3D
//   reelle, puis la pile de modificateurs pilotee par NkModifierParams.
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NKEvent.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKTime/NkClock.h"
#include "NKPlatform/NkEnv.h"

#include "NK3DModeler/Shell/NkModelerTheme.h"
#include "NK3DModeler/Shell/NkModelerScreens.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::editorkit;
using namespace nkentseu::nk3d;

namespace {

	// Les raccourcis de la modelisation. UNE SEULE table : les menus de la vue,
	// le menu contextuel, la palette de recherche et le panneau T la liront tous.
	// Une liste ecrite deux fois finit toujours par diverger, et c'est
	// l'utilisateur qui le decouvre.
	void FillShortcuts(NkShortcutTable &t) {
		t.Bind("objet.deplacer", "Deplacer", NkKey::NK_G, 0, NK_SCTX_OBJECT);
		t.Bind("objet.tourner", "Tourner", NkKey::NK_R, 0, NK_SCTX_OBJECT);
		t.Bind("objet.echelle", "Redimensionner", NkKey::NK_S, 0, NK_SCTX_OBJECT);
		t.Bind("objet.dupliquer", "Dupliquer", NkKey::NK_D, NK_SC_SHIFT, NK_SCTX_OBJECT);
		t.Bind("objet.supprimer", "Supprimer", NkKey::NK_X, 0, NK_SCTX_OBJECT);
		t.Bind("objet.mode_edition", "Mode edition", NkKey::NK_TAB, 0, NK_SCTX_OBJECT);

		t.Bind("edit.extruder", "Extruder la region", NkKey::NK_E, 0, NK_SCTX_EDIT);
		t.Bind("edit.inserer", "Inserer une face", NkKey::NK_I, 0, NK_SCTX_EDIT);
		t.Bind("edit.biseauter", "Biseauter", NkKey::NK_B, NK_SC_CTRL, NK_SCTX_EDIT);
		t.Bind("edit.fusionner", "Fusionner", NkKey::NK_M, 0, NK_SCTX_EDIT);
		t.Bind("edit.subdiviser", "Subdiviser", NkKey::NK_W, 0, NK_SCTX_EDIT);
		t.Bind("edit.mode_objet", "Mode objet", NkKey::NK_TAB, 0, NK_SCTX_EDIT);

		t.Bind("app.palette", "Rechercher une commande", NkKey::NK_F3, 0, NK_SCTX_GLOBAL);
		t.Bind("app.panneau_outils", "Panneau d'outils", NkKey::NK_T, 0, NK_SCTX_GLOBAL);
		t.Bind("app.annuler", "Annuler", NkKey::NK_Z, NK_SC_CTRL, NK_SCTX_GLOBAL);
		t.Bind("app.refaire", "Refaire", NkKey::NK_Y, NK_SC_CTRL, NK_SCTX_GLOBAL);
	}

} // namespace

int nkmain(const NkEntryState &entry) {
	(void)entry;

	// ── THEMES ──────────────────────────────────────────────────────────────
	NkModelerRoles roles;
	roles.Register();

	NkThemeLibrary themes;
	NkString userThemes;
	if (const char *appdata = env::GetEnvVar("APPDATA")) {
		if (*appdata) {
			userThemes = NkString(appdata);
			userThemes.Append("/NK3DModeler/themes");
		}
	}
	const uint32 fromDisk = LoadThemes(themes, roles, "data/themes", userThemes.CStr());
	themes.SetCurrent("Sombre");

	NkThemeIssue issue{};
	if (const uint32 bad = themes.Current().Validate(&issue)) {
		printf("[theme] %u paire(s) sous le seuil : %s sur %s = %.2f (exige %.1f)\n", bad,
			   NkRoleName(issue.fg), NkRoleName(issue.bg), (double)issue.ratio, (double)issue.required);
	}

	// ── RACCOURCIS ──────────────────────────────────────────────────────────
	NkShortcutTable shortcuts;
	FillShortcuts(shortcuts);
	if (const uint32 c = shortcuts.ConflictCount())
		printf("[raccourcis] %u conflit(s).\n", c);
	printf("[nk3d] %u themes (%u depuis le disque), %u raccourcis.\n", themes.Count(), fromDisk,
		   shortcuts.Count());

	// ── FENETRE ─────────────────────────────────────────────────────────────
	// SANS CADRE OS : la maquette porte ses propres boutons de fenetre dans la
	// barre de menus. Garder le cadre natif donnerait deux barres de titre.
	NkWindowConfig wc;
	wc.title = "NK3DModeler";
	wc.width = 1600;
	wc.height = 900;
	wc.minWidth = 1100;
	wc.minHeight = 700;
	wc.centered = true;
	wc.resizable = true;
	wc.frame = false;

	NkWindow window;
	if (!window.Create(wc)) {
		printf("[nk3d] impossible de creer la fenetre.\n");
		return 1;
	}

	NkEditorCanvasRenderer renderer;
	if (!renderer.Init(window, NkEditorGfxApi::Auto)) {
		printf("[nk3d] impossible d'initialiser le rendu.\n");
		return 1;
	}

	nkgui::NkGuiContext ui;
	ui.Init((int32)wc.width, (int32)wc.height);

	nkgui::NkGuiFont font;
	if (!font.LoadEmbedded(NkEmbeddedFontId::Inter, 13.f)) {
		printf("[nk3d] police introuvable.\n");
		return 1;
	}
	ui.font = &font;
	// L'atlas de glyphes doit etre televerse AVANT la premiere frame, sinon le
	// texte sort en rectangles vides -- symptome classique et deroutant.
	renderer.UploadFontGray8(font.TexId(), font.pixels, font.atlasW, font.atlasH);

	// ── ICONES ──────────────────────────────────────────────────────────────
	// Apres la police : leurs identifiants de texture partent APRES celui de
	// l'atlas de glyphes, sinon la premiere icone ecraserait la police.
	NkModelerIcons icons;
	icons.Load(renderer, font.TexId() + 16u, 16);
	printf("[nk3d] %u icones chargees.\n", icons.LoadedCount());

	// ── ETAT ────────────────────────────────────────────────────────────────
	// Le MODE est un indice dans la liste des modes, plus un booleen : la liste
	// s'allongera (sculpt 2.5D, sculpt reel, texturing, rigging), et un booleen
	// aurait cesse de suffire au troisieme.
	int32 mode = 0; // 0 objet, 1 edition, 2 sculpt 2.5D, 3 sculpt, 4 texturing
	// Defilements. Poses ici et non dans les fonctions de peinture : ce sont des
	// etats de session, ils doivent survivre a la frame.
	float32 scrollHier = 0.f, scrollProps = 0.f, scrollDetails = 0.f;
	float32 scrollTree = 0.f, scrollAssets = 0.f;
	static const char *const kScenes[] = {"Scene_01", "Scene_02"};

	NkClock clock;
	uint32 lastW = wc.width, lastH = wc.height;

	// ── BOUCLE ──────────────────────────────────────────────────────────────
	while (window.IsOpen()) {
		while (NkEvent *ev = NkEvents().PollEvent()) {
			(void)ev;
		}

		const math::NkVec2u sz = window.GetSize();
		if (sz.x > 0 && sz.y > 0 && (sz.x != lastW || sz.y != lastH)) {
			renderer.OnResize(sz.x, sz.y);
			lastW = sz.x;
			lastH = sz.y;
		}

		float32 dt = clock.Tick().delta;
		if (dt <= 0.f || dt > 0.1f)
			dt = 1.f / 60.f;

		ui.BeginFrame(dt);

		const float32 W = (float32)lastW, H = (float32)lastH;
		NkLayout lay;
		lay.Compute(W, H);

		const NkTheme &theme = themes.Current();
		NkModelerPainter p(ui.dl, font, theme, roles, icons);

		// Fond general : il se voit dans les interstices entre panneaux, et c'est
		// ce qui donne la profondeur a trois niveaux de UI_SPEC 10bis.1.
		p.Fill({0.f, 0.f, W, H}, NkRole::WindowBg);

		PaintMenuBar(p, lay.menu, "MonProjet");
		PaintTabs(p, lay.tabs, kScenes, 2, 0);
		const bool editMode = (mode == 1);
		PaintToolbar(p, lay.tool, mode);
		PaintHierarchy(p, lay.left, 1, scrollHier);
		PaintViewport(p, lay.view, editMode, shortcuts);
		PaintProperties(p, lay.propsR, scrollProps);
		PaintDetails(p, lay.detailsR, scrollDetails);
		PaintBrowser(p, lay.browser, scrollTree, scrollAssets);
		PaintStatus(p, lay.status,
					editMode ? "Sommets 8 - Aretes 12 - Faces 6 - sel. 3 faces - 60 ips"
							 : "Objets 6 - selectionne : Cube - 60 ips");

		ui.EndFrame();

		renderer.BeginFrame();
		renderer.SubmitDrawList(ui.dl, lastW, lastH);
		renderer.EndFrame();
	}

	renderer.Shutdown();
	ui.Shutdown();
	return 0;
}
