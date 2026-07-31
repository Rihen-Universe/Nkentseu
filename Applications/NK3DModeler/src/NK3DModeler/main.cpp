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
#include "NKEvent/NkMouseEvent.h"

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

	// ── LA TAILLE DE REFERENCE EST CELLE DU RENDU, PAS CELLE DE LA FENETRE ──
	// C'ETAIT LA CAUSE DU FLOU. J'initialisais l'interface avec la taille
	// DEMANDEE (1600x900) alors que la fenetre reelle fait 1616x939 : la liste de
	// dessin etait projetee dans un repere qui ne correspondait pas au tampon de
	// rendu, et toute l'image se retrouvait reechantillonnee -- texte et icones
	// compris. D'ou le flou uniforme et la fatigue visuelle.
	//
	// On interroge desormais le RENDU et non la fenetre : lui seul sait la taille
	// de son tampon. La fenetre peut compter ses bordures, l'OS peut ajuster, et
	// les deux chiffres divergent sans prevenir.
	math::NkVec2u real0 = renderer.Size();
	if (real0.x == 0 || real0.y == 0)
		real0 = window.GetSize(); // repli : mieux vaut une taille approchee qu'une taille nulle
	printf("[nk3d] fenetre %ux%u, tampon de rendu %ux%u\n", window.GetSize().x, window.GetSize().y,
		   real0.x, real0.y);

	nkgui::NkGuiContext ui;
	ui.Init((int32)real0.x, (int32)real0.y);

	// ── ECHELLE D'INTERFACE ─────────────────────────────────────────────────
	// Sans elle, sur un ecran a 125 % ou 150 %, Windows ETIRE l'image de la
	// fenetre : tout devient mou. C'est la premiere cause du flou signale.
	float32 uiScale = window.GetDpiScale();
	if (uiScale < 0.5f || uiScale > 4.f)
		uiScale = 1.f; // valeur aberrante : on prefere une interface petite a une interface cassee

	// ── DENSITE D'INTERFACE ─────────────────────────────────────────────────
	// Facteur de LISIBILITE, distinct du DPI, et il a une raison technique.
	//
	// Dans NkGuiDrawList::AddText, le curseur avance de `g->advance`, qui est
	// FRACTIONNAIRE. Seul le PREMIER glyphe d'une chaine tombe donc sur un pixel
	// entier ; les suivants derivent, et chacun echantillonne l'atlas ENTRE deux
	// texels. C'est structurel, et partage avec NKCode -- on ne le corrige pas
	// depuis ici.
	//
	// Mais l'erreur est un demi-texel CONSTANT : a 13 px de corps elle represente
	// 4 % de la hauteur d'un caractere, a 15 px seulement 3,3 %. Grossir le texte
	// ne supprime pas le defaut, il en DIVISE l'effet -- et c'est pour cela que
	// le shell de NKCode charge Inter a 16 px et non a 13. Rihen a donc vu juste
	// en soupconnant l'echelle.
	//
	// 1,15 est un compromis : assez pour que le texte se pose, pas au point de
	// faire perdre deux lignes a chaque panneau. Ce sera le curseur « densite »
	// des reglages.
	const float32 kUiZoom = 1.15f;
	const float32 total = uiScale * kUiZoom;
	ApplyUiScale(total);
	printf("[nk3d] echelle : DPI %.2f x densite %.2f = %.2f\n", (double)uiScale, (double)kUiZoom,
		   (double)total);

	nkgui::NkGuiFont font;
	// La taille de corps est ARRONDIE : une police demandee a 16,25 px produit
	// des metriques fractionnaires, donc des lignes de base entre deux pixels.
	const float32 fontPx = (float32)(int32)(13.f * total + 0.5f);
	if (!font.LoadEmbedded(NkEmbeddedFontId::Inter, fontPx)) {
		printf("[nk3d] police introuvable.\n");
		return 1;
	}
	printf("[nk3d] police Inter a %.0f px.\n", (double)fontPx);
	ui.font = &font;
	// L'atlas de glyphes doit etre televerse AVANT la premiere frame, sinon le
	// texte sort en rectangles vides -- symptome classique et deroutant.
	renderer.UploadFontGray8(font.TexId(), font.pixels, font.atlasW, font.atlasH);

	// ── ICONES ──────────────────────────────────────────────────────────────
	// Apres la police : leurs identifiants de texture partent APRES celui de
	// l'atlas de glyphes, sinon la premiere icone ecraserait la police.
	NkModelerIcons icons;
	icons.Load(renderer, font.TexId() + 16u, (int32)(16.f * total + 0.5f));
	printf("[nk3d] %u icones chargees.\n", icons.LoadedCount());

	// ── ETAT DE SESSION ─────────────────────────────────────────────────────
	NkModelerState st;
	NkHitRegistry hit;
	NkWidgetState ws;
	NkComboPending combo;
	static const char *const kScenes[] = {"Scene_01", "Scene_02"};

	// ── ENTREE SOURIS ───────────────────────────────────────────────────────
	// NKGui calcule les TRANSITIONS (clic, relachement, double-clic) dans
	// BeginFrame a partir de l'etat BRUT que l'application pose ici. On se
	// contente donc de reporter les evenements ; c'est BeginFrame qui en tire
	// « vient d'etre clique ».
	{
		auto &ev = NkEvents();
		ev.AddEventCallback<NkMouseMoveEvent>([&ui](NkMouseMoveEvent *e) {
			ui.input.mousePos = {(float32)e->GetX(), (float32)e->GetY()};
		});
		ev.AddEventCallback<NkMouseButtonPressEvent>([&ui](NkMouseButtonPressEvent *e) {
			const NkMouseButton b = e->GetButton();
			if (b == NkMouseButton::NK_MB_LEFT)
				ui.input.mouseDown[0] = true;
			else if (b == NkMouseButton::NK_MB_RIGHT)
				ui.input.mouseDown[1] = true;
			else if (b == NkMouseButton::NK_MB_MIDDLE)
				ui.input.mouseDown[2] = true;
			ui.input.ctrlDown = e->GetModifiers().ctrl;
			ui.input.shiftDown = e->GetModifiers().shift;
			ui.input.altDown = e->GetModifiers().alt;
		});
		ev.AddEventCallback<NkMouseButtonReleaseEvent>([&ui](NkMouseButtonReleaseEvent *e) {
			const NkMouseButton b = e->GetButton();
			if (b == NkMouseButton::NK_MB_LEFT)
				ui.input.mouseDown[0] = false;
			else if (b == NkMouseButton::NK_MB_RIGHT)
				ui.input.mouseDown[1] = false;
			else if (b == NkMouseButton::NK_MB_MIDDLE)
				ui.input.mouseDown[2] = false;
		});
		// La molette s'ACCUMULE : plusieurs crans peuvent arriver dans la meme
		// frame, et n'en garder qu'un rendrait le defilement saccade.
		ev.AddEventCallback<NkMouseWheelVerticalEvent>(
			[&ui](NkMouseWheelVerticalEvent *e) { ui.input.wheel += (float32)e->GetDeltaY(); });
		ev.AddEventCallback<NkWindowCloseEvent>([&st](NkWindowCloseEvent *) { st.running = false; });
	}

	NkClock clock;
	uint32 lastW = real0.x, lastH = real0.y;

	// ── BOUCLE ──────────────────────────────────────────────────────────────
	while (st.running && window.IsOpen()) {
		while (NkEvent *ev = NkEvents().PollEvent()) {
			(void)ev;
		}

		// On previent le rendu du changement, PUIS on relit SA taille : c'est elle
		// qui sert a projeter, pas celle qu'on vient de lui donner.
		const math::NkVec2u winSz = window.GetSize();
		if (winSz.x > 0 && winSz.y > 0 && (winSz.x != lastW || winSz.y != lastH)) {
			renderer.OnResize(winSz.x, winSz.y);
			const math::NkVec2u rs = renderer.Size();
			lastW = rs.x > 0 ? rs.x : winSz.x;
			lastH = rs.y > 0 ? rs.y : winSz.y;
		}

		float32 dt = clock.Tick().delta;
		if (dt <= 0.f || dt > 0.1f)
			dt = 1.f / 60.f;

		ui.BeginFrame(dt);
		// Le registre est reinitialise APRES BeginFrame : il lit les transitions
		// que celui-ci vient de calculer.
		hit.Begin(ui.input);
		// Relu CHAQUE frame et non seulement apres notre bouton : l'utilisateur peut
		// maximiser par double-clic sur la barre, par raccourci Windows ou en glissant
		// la fenetre en haut de l'ecran. L'icone doit suivre dans tous les cas.
		st.maximized = window.IsMaximized();

		const float32 W = (float32)lastW, H = (float32)lastH;
		NkLayout lay;
		lay.Compute(W, H, st.leftFrac, st.rightFrac, st.browserFrac, st.propsFrac);

		const NkTheme &theme = themes.Current();
		NkModelerPainter p(ui.dl, font, theme, roles, icons);

		// Fond general : il se voit dans les interstices entre panneaux, et c'est
		// ce qui donne la profondeur a trois niveaux de UI_SPEC 10bis.1.
		p.Fill({0.f, 0.f, W, H}, NkRole::WindowBg);

		PaintMenuBarI(p, lay.menu, "MonProjet", st, hit);
		PaintTabsI(p, lay.tabs, st, hit, ws, ui.input);
		PaintToolbar(p, lay.tool, st, hit, ws, combo);
		PaintHierarchy(p, lay.left, st, hit, ws, ui.input);
		PaintViewport(p, lay.view, st, hit, ws, combo, shortcuts);
		PaintProperties(p, lay.propsR, st, hit, ws, ui.input);
		PaintDetails(p, lay.detailsR, st, hit);
		PaintBrowser(p, lay.browser, st, hit, ws, ui.input);
		PaintStatus(p, lay.status, st);

		// L'ORDRE DE CES TROIS APPELS EST SIGNIFIANT. Les separateurs doivent
		// recevoir le clic avant les panneaux qu'ils bordent ; le menu deroule
		// recouvre tout ; la boite de confirmation recouvre le menu. Le registre
		// donnant la priorite a la DERNIERE zone declaree, l'ordre de peinture EST
		// l'ordre de priorite -- il n'y a rien d'autre a synchroniser.
		// La liste deroulee est peinte AVANT les separateurs et le menu : elle doit
		// les recouvrir, et le registre donne la priorite a la derniere zone.
		PaintSplitters(p, lay, W, H, st, hit);
		DrawComboPopup(p, hit, ws, combo);
		PaintOpenMenu(p, lay.menu, st, hit, shortcuts);
		PaintCloseDialog(p, W, H, st, hit);

		ui.EndFrame();

		// ── CURSEUR ─────────────────────────────────────────────────────────
		// Repose CHAQUE frame : sur Windows le systeme le remet a la fleche des
		// que la souris traverse une zone qui ne le redemande pas.
		switch (hit.Cursor()) {
			case NkCursorWant::ResizeWE:
				window.SetCursor(NkWindow::NkCursorType::ResizeWE);
				break;
			case NkCursorWant::ResizeNS:
				window.SetCursor(NkWindow::NkCursorType::ResizeNS);
				break;
			case NkCursorWant::Hand:
				window.SetCursor(NkWindow::NkCursorType::Hand);
				break;
			default:
				window.SetCursor(NkWindow::NkCursorType::Arrow);
				break;
		}

		renderer.BeginFrame();
		renderer.SubmitDrawList(ui.dl, lastW, lastH);
		renderer.EndFrame();

		// ── ACTIONS DE FENETRE, HORS FRAME ──────────────────────────────────
		// BeginDragMove et Maximize entrent dans une boucle modale de l'OS : les
		// appeler pendant la peinture reentrerait dans la frame. On les consomme
		// donc ICI, une fois l'image envoyee.
		if (st.wantMinimize) {
			st.wantMinimize = false;
			window.Minimize();
		}
		if (st.wantMaxRestore) {
			st.wantMaxRestore = false;
			if (window.IsMaximized())
				window.Restore();
			else
				window.Maximize();
			st.maximized = window.IsMaximized();
		}
		if (st.wantDragMove) {
			st.wantDragMove = false;
			// Un deplacement n'a pas de sens sur une fenetre maximisee : Windows la
			// restaurerait a mi-course, ce qui donne un saut desagreable.
			if (!window.IsMaximized())
				window.BeginDragMove();
		}
	}

	renderer.Shutdown();
	ui.Shutdown();
	return 0;
}
