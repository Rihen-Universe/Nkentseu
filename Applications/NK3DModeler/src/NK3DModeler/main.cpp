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
#include "NKGui/NkEditorRHIRenderer.h" // Integrations/NKGui
#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h" // PORTAGE INTEGRAL de --demo=2
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

	// RENDU SUR NKRHI, PAS SUR NKCANVAS. Meme interface NkIEditorRenderer, donc
	// l'interface 2D ne change pas d'une ligne ; ce qui change, c'est que ce
	// renderer-ci EXPOSE SON DEVICE. C'est la condition pour que la vue 3D
	// (NKRenderer) rende dans une cible hors ecran sur le MEME device et que sa
	// texture soit simplement posee dans la draw-list -- au lieu d'ouvrir une
	// seconde pile GPU dans la meme fenetre, ce que le depot interdit
	// explicitement (« une fenetre = une pile »).
	// La scene 3D rend dans sa cible hors ecran sur le command buffer de
	// l'editeur, AVANT que la passe backbuffer ne s'ouvre : on ne peut pas
	// imbriquer une passe de rendu dans une autre. Puis on publie sa texture
	// aupres du backend, pour que la draw-list n'ait plus qu'a la poser.
	static auto preUI3D = [](NkICommandBuffer *cmd, void *user) {
		auto *r = static_cast<nkgui::NkEditorRHIRenderer *>(user);
		// PORTAGE INTEGRAL de --demo=2 : la vue 3D est desormais la demo de
		// renderdemo, portee telle quelle (NkDemo3D.cpp). L'ancienne vue
		// (NkViewport3D) reste compilee mais DORMANTE — on ne lui donne plus
		// de device, donc chacun de ses appels est un no-op sans danger.
		demo::Demo3DHostFrame(cmd);
		demo::Demo3DHostRegisterInto(&r->GetBackend());
	};

	nkgui::NkEditorRHIRenderer renderer;
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
	// Le device de l'interface EST celui de la vue 3D. C'est toute la raison
	// d'avoir quitte NKCanvas : deux piles GPU dans une fenetre, le depot
	// l'interdit, et une relecture CPU par image serait hors de question.
	// L'ancienne vue ne recoit VOLONTAIREMENT plus le device : c'est ce qui la
	// rend dormante (son Init3D echoue proprement et chaque facade se tait).
	demo::Demo3DHostSetDevice(renderer.GetDevice());
	renderer.SetPreUI(preUI3D, &renderer);

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

	// VIGNETTES DES MATCAPS (ids 4300+) : la bibliotheque genere chaque
	// boule en pixels, et on l'uploade comme n'importe quelle icone. C'est
	// l'APERCU REEL du selecteur -- un pictogramme generique ne dit pas
	// quelle matiere on choisit.
	{
		// 128 px, pas 32 : le grand apercu du panneau fait 72 px, et AGRANDIR
		// pixelise (constate par Rihen) -- on genere plus grand que le plus
		// grand usage ; le retrecissement, lui, reste lisse.
		static uint8 ball[128 * 128 * 4];
		const int32 nMc = demo::Demo3DHostMatcapCount();
		for (int32 i = 0; i < nMc; ++i) {
			demo::Demo3DHostMatcapBall(i, ball, 128);
			renderer.UploadImageRGBA(4300u + (uint32)i, ball, 128, 128);
		}
		printf("[nk3d] %d vignettes de matcap (128 px).\n", nMc);
	}

	// ── ETAT DE SESSION ─────────────────────────────────────────────────────
	NkModelerState st;
	NkHitRegistry hit;
	NkWidgetState ws;
	NkComboPending combo;
	NkCheckPending checks;
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
		// ── CLAVIER ─────────────────────────────────────────────────────────
		// Une quinzaine de fonctions de la vue 3D etaient ecrites mais DORMANTES :
		// aucun appelant. Le clavier est leur premier chemin d'acces -- les menus
		// et la palette suivront, alimentes par la meme table de raccourcis.
		//
		// Le clavier de Blender, parce que c'est celui que connaissent les gens qui
		// modelisent. Les touches ne sont PAS ecrites en dur ailleurs : cette table
		// est le seul endroit ou l'on decide « quelle touche fait quoi ».
		//
		// Les evenements arrivent HORS de la frame, donc on ne touche pas au
		// maillage ici : on pose une intention, consommee dans la boucle. Modifier
		// la geometrie depuis un callback reentrerait dans une image en cours de
		// peinture, avec des tampons a moitie ecrits.
		// ── SAISIE DE TEXTE ─────────────────────────────────────────────────
		// RIEN DE TOUT CECI N'ETAIT BRANCHE. NkGuiInput expose PushChar et SetKey,
		// mais l'application ne les appelait jamais : les champs de saisie ne
		// recevaient donc aucun caractere, aucune touche Entree, aucun Echap. D'ou
		// « impossible de renommer » ET « impossible de fermer l'editeur » -- ce
		// n'etait pas deux bugs mais un seul, en amont de tout le reste.
		ev.AddEventCallback<NkTextInputEvent>([&ui](NkTextInputEvent *e) {
			ui.input.PushChar(e->GetCodepoint());
		});
		// Les touches d'EDITION ont leur propre table dans NKGui, distincte des
		// raccourcis de l'application : c'est ce qui permet a Entree de valider un
		// nom sans declencher aussi une commande.
		ev.AddEventCallback<NkKeyPressEvent>([&ui](NkKeyPressEvent *e) {
			switch (e->GetKey()) {
				case NkKey::NK_ENTER:
				case NkKey::NK_NUMPAD_ENTER:
					ui.input.SetKey(nkgui::NkGuiKey::Enter, true);
					break;
				case NkKey::NK_ESCAPE:
					ui.input.SetKey(nkgui::NkGuiKey::Escape, true);
					break;
				case NkKey::NK_BACK:
					ui.input.SetKey(nkgui::NkGuiKey::Backspace, true);
					break;
				case NkKey::NK_DELETE:
					ui.input.SetKey(nkgui::NkGuiKey::Delete, true);
					break;
				case NkKey::NK_LEFT:
					ui.input.SetKey(nkgui::NkGuiKey::Left, true);
					break;
				case NkKey::NK_RIGHT:
					ui.input.SetKey(nkgui::NkGuiKey::Right, true);
					break;
				default:
					break;
			}
		});
		ev.AddEventCallback<NkKeyReleaseEvent>([&ui](NkKeyReleaseEvent *e) {
			switch (e->GetKey()) {
				case NkKey::NK_ENTER:
				case NkKey::NK_NUMPAD_ENTER:
					ui.input.SetKey(nkgui::NkGuiKey::Enter, false);
					break;
				case NkKey::NK_ESCAPE:
					ui.input.SetKey(nkgui::NkGuiKey::Escape, false);
					break;
				case NkKey::NK_BACK:
					ui.input.SetKey(nkgui::NkGuiKey::Backspace, false);
					break;
				case NkKey::NK_DELETE:
					ui.input.SetKey(nkgui::NkGuiKey::Delete, false);
					break;
				case NkKey::NK_LEFT:
					ui.input.SetKey(nkgui::NkGuiKey::Left, false);
					break;
				case NkKey::NK_RIGHT:
					ui.input.SetKey(nkgui::NkGuiKey::Right, false);
					break;
				default:
					break;
			}
		});

		ev.AddEventCallback<NkKeyPressEvent>([&st](NkKeyPressEvent *e) {
			const NkKey k = e->GetKey();
			const auto mods = e->GetModifiers();
			const bool ctrl = mods.ctrl, shift = mods.shift, alt = mods.alt;
			// La saisie d'un nom en cours capte TOUT : taper « e » dans un champ ne
			// doit pas extruder. C'est le premier reflexe a avoir des qu'un
			// raccourci d'une seule lettre existe.
			if (st.editingText)
				return;
			auto want = [&st](NkVpAction a) { st.pendingAction = a; };

			switch (k) {
				// ── Modes ───────────────────────────────────────────────────
				case NkKey::NK_TAB:
					want(NkVpAction::ToggleEdit);
					break;
				case NkKey::NK_NUM1:
					want(NkVpAction::SubModeVertex);
					break;
				case NkKey::NK_NUM2:
					want(NkVpAction::SubModeEdge);
					break;
				case NkKey::NK_NUM3:
					want(NkVpAction::SubModeFace);
					break;
				// ── Selection ───────────────────────────────────────────────
				case NkKey::NK_A:
					want(alt ? NkVpAction::SelectNone : NkVpAction::SelectAll);
					break;
				// ── Outils de transformation ────────────────────────────────
				// G / R / S ARMENT UNE TRANSFORMATION, ils ne changent pas d'outil.
				// C'est le geste de Blender : la touche saisit l'objet, la souris le
				// pilote, X / Y / Z contraignent, le clic confirme et Echap annule.
				// Le choisir plutot que « selectionner l'outil » n'est pas un detail :
				// il n'y a aucune poignee a viser, donc rien a rater.
				case NkKey::NK_G:
					want(NkVpAction::ModalMove);
					break;
				case NkKey::NK_R:
					want(ctrl ? NkVpAction::LoopCut : NkVpAction::ModalRotate);
					break;
				case NkKey::NK_S:
					want(NkVpAction::ModalScale);
					break;
				// ── Operations ──────────────────────────────────────────────
				case NkKey::NK_E:
					want(shift ? NkVpAction::ExtrudeIndividual : NkVpAction::Extrude);
					break;
				case NkKey::NK_X:
					// Pendant une modale, X contraint a l'axe ; sinon il supprime.
					// L'intention posee est « axe X », et le dispatch la reinterprete
					// en suppression s'il n'y a pas de modale en cours.
					want(ctrl ? NkVpAction::Dissolve : NkVpAction::ModalAxisX);
					break;
				case NkKey::NK_M:
					want(NkVpAction::Merge);
					break;
				case NkKey::NK_F:
					want(NkVpAction::MakeFace);
					break;
				case NkKey::NK_W:
					want(NkVpAction::Subdivide);
					break;
				case NkKey::NK_I:
					want(NkVpAction::Inset);
					break;
				case NkKey::NK_B:
					// Ctrl+B biseaute, B seul arme la selection RECTANGLE -- c'est le
					// clavier de Blender, ou B veut dire « box select ».
					want(ctrl ? (shift ? NkVpAction::BevelVertex : NkVpAction::BevelEdge)
							  : NkVpAction::ZoneRect);
					break;
				case NkKey::NK_C:
					// C arme la selection CERCLE (peinture) ; la molette en regle le
					// rayon pendant le geste.
					want(NkVpAction::ZoneCircle);
					break;
				// ── Annulation ──────────────────────────────────────────────
				case NkKey::NK_Z:
					if (ctrl)
						want(shift ? NkVpAction::Redo : NkVpAction::Undo);
					else if (alt)
						want(NkVpAction::ToggleXray);
					else
						want(NkVpAction::ModalAxisZ); // contrainte, si une modale court
					break;
				// ── Contraintes d'axe et fin de transformation ──────────────
				// Ces touches N'ONT DE SENS QUE pendant une modale ; hors modale,
				// elles retombent sur leur role habituel (X = supprimer). C'est le
				// dispatch qui tranche, pas le callback : lui ne connait pas l'etat
				// de la vue.
				case NkKey::NK_Y:
					want(ctrl ? NkVpAction::Redo : NkVpAction::ModalAxisY);
					break;
				case NkKey::NK_ESCAPE:
					want(NkVpAction::ModalCancel);
					break;
				case NkKey::NK_ENTER:
					want(NkVpAction::ModalConfirm);
					break;

				// ── Vues du pave numerique ──────────────────────────────────
				// Ctrl donne la vue OPPOSEE, comme chez Blender : c'est deux fois
				// moins de touches a retenir pour six vues.
				case NkKey::NK_NUMPAD_1:
					want(ctrl ? NkVpAction::ViewBack : NkVpAction::ViewFront);
					break;
				case NkKey::NK_NUMPAD_3:
					want(ctrl ? NkVpAction::ViewLeft : NkVpAction::ViewRight);
					break;
				case NkKey::NK_NUMPAD_7:
					want(ctrl ? NkVpAction::ViewBottom : NkVpAction::ViewTop);
					break;
				case NkKey::NK_NUMPAD_5:
					want(NkVpAction::ToggleOrtho);
					break;
				case NkKey::NK_NUMPAD_DOT:
					want(NkVpAction::FrameAll);
					break;
				case NkKey::NK_HOME:
					want(NkVpAction::FrameAll);
					break;
				default:
					break;
			}
		});

		ev.AddEventCallback<NkMouseDoubleClickEvent>([&ui](NkMouseDoubleClickEvent *e) {
			const NkMouseButton b = e->GetButton();
			ui.input.SetDoubleClick(b == NkMouseButton::NK_MB_LEFT
										? 0
										: (b == NkMouseButton::NK_MB_RIGHT ? 1 : 2));
		});

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
		// Memorise AVANT le Begin (qui reinitialise le registre) : ce que la
		// souris survolait a l'image precedente. Le pilotage du gizmo en a besoin
		// pour distinguer « clic sur la scene » de « clic sur un widget pose
		// par-dessus la scene ».
		const bool overSceneLastFrame = hit.IsHovered("view.nav");
		hit.Begin(ui.input);
		// La garde du clavier suit l'etat REEL des widgets : tant qu'un champ est
		// en cours de saisie, aucune touche ne doit atteindre les raccourcis.
		st.editingText = ws.editing;
		// Relu CHAQUE frame et non seulement apres notre bouton : l'utilisateur peut
		// maximiser par double-clic sur la barre, par raccourci Windows ou en glissant
		// la fenetre en haut de l'ecran. L'icone doit suivre dans tous les cas.
		st.maximized = window.IsMaximized();

		const float32 W = (float32)lastW, H = (float32)lastH;
		NkLayout lay;
		lay.Compute(W, H, st.leftFrac, st.rightFrac, st.browserFrac, st.propsFrac, st.showLeft,
					st.showRight, st.showBrowser);

		// Bornes des panneaux deroulants pour cette image.
		NkPopupBoundsW() = W;
		NkPopupBoundsH() = H;

		// ── L'INTERFACE PILOTE LA VUE 3D ────────────────────────────────────
		// Tout descend ici, AVANT la peinture : la barre de la vue est lue a
		// l'image N et appliquee a l'image N. L'inverse -- appliquer apres avoir
		// peint -- ferait toujours voir l'etat precedent, ce qui donne une
		// interface qui « repond en retard » sans qu'on sache pourquoi.
		nk3d::Viewport3DSetShading(st.shading, st.solidLight);
		nk3d::Viewport3DSetOverlays(st.overlayMask);
		nk3d::Viewport3DResize((uint32)lay.view.w, (uint32)lay.view.h);
		// La demo portee recoit la taille de la vue, son origine (traduction
		// souris fenetre -> vue), le survol (ses raccourcis n'ecoutent que la
		// vue survolee, comme Blender) et la garde de saisie de texte.
		// AUCUNE pastille de proprietes active : le panneau se REPLIE sur sa
		// colonne de pastilles et la VUE recupere la place.
		if (st.showRight && !(st.propOpen0 || st.propOpen1 || st.propOpen2)) {
			const float32 tabW = S(28.f);
			const float32 give = lay.propsR.w - tabW;
			if (give > 0.f) {
				lay.view.w += give;
				lay.propsR.x += give;
				lay.propsR.w = tabW;
				lay.detailsR.x += give;
				lay.detailsR.w = tabW;
			}
		}
		// La vue REELLE vit SOUS la barre d'espaces : taille et origine de la
		// souris doivent viser la meme zone que l'image, sinon le picking
		// decale d'une hauteur de barre.
		{
			NkRect viewImg = lay.view;
			if (st.wsBarOpen) {
				viewImg.y += S(24.f);
				viewImg.h -= S(24.f);
			}
			demo::Demo3DHostResize((uint32)viewImg.w, (uint32)viewImg.h);
			demo::Demo3DHostSetView(viewImg.x, viewImg.y, overSceneLastFrame, !st.editingText);
		}

		// ── SYNCHRONISATION UI <-> DEMO PORTEE ──────────────────────────────
		// POUSSER quand l'interface a change depuis l'image precedente, TIRER
		// sinon : les raccourcis de la demo (Z, virgule, pave numerique,
		// Shift+TAB...) restent maitres et l'interface les REFLETE, au lieu de
		// les ecraser chaque image. L'etat « derniere valeur vue » vit ici.
		if (demo::Demo3DHostReady()) {
			static struct {
					int32 shading = -1, solidLight = -1, projection = -1, orientation = -1,
						  camSpeed = -1, gizmoOp = -1;
					uint32 overlay = 0xFFFFFFFFu;
					NkTool tool = (NkTool)255;
					bool snapGrid = false, snapAngle = false, snapScale = false;
					bool first = true;
			} sy;

			// Ombrage (les 6 modes reels : l'index de la liste EST le mode).
			if (!sy.first && st.shading != sy.shading)
				demo::Demo3DHostSetShading(st.shading);
			else
				st.shading = demo::Demo3DHostShading();
			sy.shading = st.shading;

			// Source de couleur des modes non eclaires (touche B de la demo).
			if (!sy.first && st.solidLight != sy.solidLight)
				demo::Demo3DHostSetUnlitColor(st.solidLight);
			else
				st.solidLight = demo::Demo3DHostUnlitColor();
			sy.solidLight = st.solidLight;

			// Projection : 0 perspective, 1 orthogonale, 2..7 vues d'axe. Une vue
			// d'axe est une ACTION (elle pose la camera) ; l'etat durable, c'est
			// ortho/perspective.
			if (!sy.first && st.projection != sy.projection) {
				if (st.projection == 0)
					demo::Demo3DHostSetOrtho(false);
				else if (st.projection == 1)
					demo::Demo3DHostSetOrtho(true);
				else {
					// Dessus/Dessous, Avant/Arriere, Gauche/Droite.
					static const int32 kWhich[6] = {2, 2, 0, 0, 1, 1};
					static const bool kOpp[6] = {false, true, false, true, true, false};
					demo::Demo3DHostAxisView(kWhich[st.projection - 2], kOpp[st.projection - 2]);
				}
			} else if (!demo::Demo3DHostIsOrtho()) {
				st.projection = 0;
			} else if (st.projection == 0) {
				st.projection = 1;
			}
			sy.projection = st.projection;
			st.lastProjection = st.projection;

			// Orientation du gizmo (monde / local / normale).
			if (!sy.first && st.orientation != sy.orientation)
				demo::Demo3DHostSetOrientation(st.orientation);
			else
				st.orientation = demo::Demo3DHostOrientation();
			sy.orientation = st.orientation;

			// Outils. Deplacer/Rotation/Echelle/Multigizmo = les 4 modes du gizmo
			// de la demo ; Selection et Curseur sont des outils du shell qui
			// s'appuient sur ses mecanismes (zones, curseur 3D).
			const int32 opNow = demo::Demo3DHostGizmoOp();
			if (!sy.first && st.tool != sy.tool) {
				if ((int32)st.tool >= (int32)NkTool::Move)
					demo::Demo3DHostSetGizmoOp((int32)st.tool - (int32)NkTool::Move);
			} else if (opNow != sy.gizmoOp && (int32)st.tool >= (int32)NkTool::Move) {
				// G/R/S/C presses dans la vue : l'outil de la barre suit.
				st.tool = (NkTool)((int32)NkTool::Move + opNow);
			}
			sy.gizmoOp = demo::Demo3DHostGizmoOp();
			sy.tool = st.tool;
			demo::Demo3DHostSetCursorTool(st.tool == NkTool::Cursor);
			demo::Demo3DHostSetZoneTool(st.tool == NkTool::Select ? st.selShape : -1);
			demo::Demo3DHostSetGizmoHidden(st.tool == NkTool::Select || st.tool == NkTool::Cursor);

			// Vitesse de camera : 1x / 2x / 4x / 8x.
			if (st.camSpeed != sy.camSpeed) {
				demo::Demo3DHostSetCamSpeed((float32)(1 << st.camSpeed));
				sy.camSpeed = st.camSpeed;
			}

			// Aimantation : les pas sont FIXES (0,5 / 15 deg / 0,1) et l'etat du
			// gizmo est GLOBAL -> la bascule appliquee est celle du mode courant.
			{
				const bool changed = st.snapGrid != sy.snapGrid || st.snapAngle != sy.snapAngle ||
									 st.snapScale != sy.snapScale;
				bool *cur = &st.snapGrid;
				if (opNow == 1)
					cur = &st.snapAngle;
				else if (opNow == 2)
					cur = &st.snapScale;
				if (!sy.first && changed)
					demo::Demo3DHostSetSnap(*cur, 0.5f, 15.f, 0.1f);
				else
					*cur = demo::Demo3DHostSnapEnabled(); // Shift+TAB dans la vue
				sy.snapGrid = st.snapGrid;
				sy.snapAngle = st.snapAngle;
				sy.snapScale = st.snapScale;
			}

			// Surimpressions : grille et ses traits (F1..F4), lisere, HUD.
			if (!sy.first && st.overlayMask != sy.overlay) {
				demo::Demo3DHostSetGridFlags((st.overlayMask & 1u) != 0u, (st.overlayMask & 2u) != 0u,
											 (st.overlayMask & 4u) != 0u, (st.overlayMask & 8u) != 0u);
				demo::Demo3DHostSetOutline((st.overlayMask & 16u) != 0u);
				demo::Demo3DHostSetHud((st.overlayMask & 32u) != 0u);
			} else {
				bool g0, g1, g2, g3;
				demo::Demo3DHostGridFlags(&g0, &g1, &g2, &g3);
				st.overlayMask = (g0 ? 1u : 0u) | (g1 ? 2u : 0u) | (g2 ? 4u : 0u) | (g3 ? 8u : 0u) |
								 (demo::Demo3DHostOutline() ? 16u : 0u) |
								 (demo::Demo3DHostHud() ? 32u : 0u);
			}
			sy.overlay = st.overlayMask;

			// Sous-mode : refleter le masque reel (le bouton pousse lui-meme).
			{
				const int32 m2 = demo::Demo3DHostEditSelMask();
				st.subMode = (m2 & 1) ? NkSubMode::Vertex : ((m2 & 2) ? NkSubMode::Edge : NkSubMode::Face);
			}

			// Premiere image : tout TIRER, ne rien pousser -- la demo est la
			// source de verite a l'ouverture. Et le HUD suit le masque du shell
			// (off par defaut : il chevauchait la barre d'outils).
			if (sy.first) {
				sy.first = false;
				demo::Demo3DHostSetHud((st.overlayMask & 32u) != 0u);
				demo::Demo3DHostSetOutline((st.overlayMask & 16u) != 0u);
				sy.overlay = 0xFFFFFFFFu; // re-tirer au prochain tour
			}
		}
		nk3d::Viewport3DSetEditMode(st.mode != NkMode::Object);
		// Le sous-mode de la vue devient le masque de selection. Un seul bit ici :
		// les trois boutons sont exclusifs. Les combiner (Maj+1/2/3 chez Blender)
		// viendra avec les raccourcis clavier.
		nk3d::Viewport3DSetSelectMask(1u << (uint32)st.subMode);
		// Outil -> mode du gizmo. « Selection » et « Curseur » n'en ont pas : on
		// laisse alors le gizmo sur le deplacement, mais il ne prendra pas le clic
		// puisque l'arbitrage donne la priorite au maillage.
		{
			int32 gm = 0;
			if (st.tool == NkTool::Rotate)
				gm = 1;
			else if (st.tool == NkTool::Scale)
				gm = 2;
			nk3d::Viewport3DSetGizmoMode(gm);
			// L'outil SELECTION ne transforme rien : afficher ses poignees ferait
			// croire le contraire, et elles captureraient les clics de selection.
			nk3d::Viewport3DSetGizmoVisible(st.tool != NkTool::Select && st.tool != NkTool::Cursor);
		}
		nk3d::Viewport3DSetGizmoOrientation(st.orientation);
		// AIMANTATION : les pas sont FIXES (0,5 unite, 15 degres, 0,1 -- ceux de
		// Demo3D) et ce sont les BASCULES qui decident si elle agit. Le gizmo n'a
		// qu'un interrupteur global : on lui donne celui de la bascule du MODE
		// COURANT -- aimanter les angles sans aimanter les positions reste ainsi
		// possible, puisqu'on ne tourne et ne deplace jamais dans le meme geste.
		// L'ancienne version passait 0 comme pas quand une bascule etait eteinte ;
		// or le gizmo IGNORE un pas nul (garde v > 0) et gardait l'ancien : la
		// valeur appliquee n'etait jamais celle qu'on croyait.
		{
			bool snapOn = st.snapGrid;
			if (st.tool == NkTool::Rotate)
				snapOn = st.snapAngle;
			else if (st.tool == NkTool::Scale)
				snapOn = st.snapScale;
			nk3d::Viewport3DSetSnap(snapOn, 0.5f, 15.f, 0.1f);
		}
		// PROJECTION : entierement geree par la SYNC de la demo portee, plus haut.
		// L'ancien bloc RELISAIT l'etat de la vue DORMANTE (Viewport3DIsOrtho,
		// toujours faux) et remettait le combo a « Perspective » une image apres
		// chaque passage en ortho -- c'est le bug « l'ortho s'active et se
		// desactive en quelques millisecondes » constate par Rihen.

		// ── ENTREE DU GIZMO ─────────────────────────────────────────────────
		// Les deplacements sont recalcules ICI, a partir de la position precedente.
		// Ne surtout pas lire un « delta » fourni par la couche d'evenements : il
		// reste fige a sa derniere valeur quand la souris s'arrete, et le gizmo
		// derive tout seul. Le probleme a deja ete rencontre dans Demo3D.
		{
			const float32 mxv = ui.input.mousePos.x - lay.view.x;
			const float32 myv = ui.input.mousePos.y - lay.view.y;
			const bool inView = (mxv >= 0.f && myv >= 0.f && mxv < lay.view.w && myv < lay.view.h);
			// LE GESTE APPARTIENT A LA ZONE OU IL A COMMENCE. Sans ce verrou, tirer
			// un champ de transformation dont le trajet traverse la vue declenchait
			// un press pour le gizmo 3D -- qui pickait dans le vide et DESELECTIONNAIT
			// l'objet qu'on etait en train de regler. De meme, un clic sur les boules
			// du gizmo de navigation (peintes PAR-DESSUS la vue) ne doit pas devenir
			// un pick 3D : on exige que le survol appartienne bien a la scene.
			if (ui.input.mouseDown[0] && !st.gizWasMouseDown)
				st.gizGestureInView = inView && overSceneLastFrame;
			if (!ui.input.mouseDown[0])
				st.gizGestureInView = false;
			st.gizWasMouseDown = ui.input.mouseDown[0];
			const bool down = ui.input.mouseDown[0] && st.gizGestureInView;
			nk3d::Viewport3DSetGizmoInput(mxv, myv, mxv - st.gizLastX, myv - st.gizLastY,
										  down && !st.gizWasDown, down, ui.input.shiftDown,
										  ui.input.ctrlDown);
			st.gizLastX = mxv;
			st.gizLastY = myv;
			st.gizWasDown = down;
		}

		// ── CONSOMMATION DE L'INTENTION CLAVIER ─────────────────────────────
		// Ici, et pas dans le callback : on est entre deux images, le maillage
		// n'est pas en cours de lecture par le rendu, et une modification
		// topologique peut donc se faire sans risque.
		// ── PILOTAGE DE LA TRANSFORMATION MODALE ────────────────────────────
		// Elle est mise a jour AVANT le dispatch : la souris a bouge depuis la
		// derniere image, et l'objet doit avoir suivi quand le panneau Proprietes
		// se peindra. C'est ce qui donne la mise a jour en TEMPS REEL, dans la vue
		// comme dans les champs.
		{
			const float32 mxv = ui.input.mousePos.x - lay.view.x;
			const float32 myv = ui.input.mousePos.y - lay.view.y;
			if (nk3d::Viewport3DModalKind() != nk3d::kVpXformNone) {
				nk3d::Viewport3DModalUpdate(mxv, myv);
				st.dirty = true;
				// Le clic gauche CONFIRME, le clic droit ANNULE -- et la modale
				// consomme le clic, sinon il tomberait ensuite sur la selection.
				if (ui.input.mouseClicked[0])
					nk3d::Viewport3DModalConfirm();
				else if (ui.input.mouseClicked[1])
					nk3d::Viewport3DModalCancel();
			}
		}

		if (st.pendingAction != NkVpAction::None) {
			const NkVpAction a = st.pendingAction;
			st.pendingAction = NkVpAction::None;
			const bool edit = (st.mode != NkMode::Object);
			const bool inModal = (nk3d::Viewport3DModalKind() != nk3d::kVpXformNone);
			const float32 mxv = ui.input.mousePos.x - lay.view.x;
			const float32 myv = ui.input.mousePos.y - lay.view.y;
			switch (a) {
				case NkVpAction::ToggleEdit:
					st.mode = edit ? NkMode::Object : NkMode::Edit;
					break;
				case NkVpAction::SubModeVertex:
					st.subMode = NkSubMode::Vertex;
					break;
				case NkVpAction::SubModeEdge:
					st.subMode = NkSubMode::Edge;
					break;
				case NkVpAction::SubModeFace:
					st.subMode = NkSubMode::Face;
					break;
				case NkVpAction::SelectAll:
					nk3d::Viewport3DSelectAll(true);
					break;
				case NkVpAction::SelectNone:
					nk3d::Viewport3DSelectAll(false);
					break;
				case NkVpAction::ToolMove:
					st.tool = NkTool::Move;
					break;
				case NkVpAction::ToolRotate:
					st.tool = NkTool::Rotate;
					break;
				case NkVpAction::ToolScale:
					st.tool = NkTool::Scale;
					break;
				// ── Modales ─────────────────────────────────────────────────
				case NkVpAction::ModalMove:
					nk3d::Viewport3DBeginModal(nk3d::kVpXformMove, mxv, myv);
					break;
				case NkVpAction::ModalRotate:
					nk3d::Viewport3DBeginModal(nk3d::kVpXformRotate, mxv, myv);
					break;
				case NkVpAction::ModalScale:
					nk3d::Viewport3DBeginModal(nk3d::kVpXformScale, mxv, myv);
					break;
				case NkVpAction::ModalAxisX:
					// HORS MODALE, X garde son role de suppression : une touche ne
					// doit pas devenir muette parce qu'un autre mode existe.
					if (inModal) {
						nk3d::Viewport3DModalAxis(0);
					} else if (edit) {
						if (nk3d::Viewport3DDeleteSelection())
							st.dirty = true;
					} else {
						const int32 act = nk3d::Viewport3DActiveObject();
						if (act >= 0) {
							nk3d::Viewport3DDeleteObject(act);
							st.dirty = true;
						}
					}
					break;
				case NkVpAction::ModalAxisY:
					if (inModal)
						nk3d::Viewport3DModalAxis(1);
					break;
				case NkVpAction::ModalAxisZ:
					if (inModal)
						nk3d::Viewport3DModalAxis(2);
					break;
				case NkVpAction::ModalConfirm:
					if (inModal)
						nk3d::Viewport3DModalConfirm();
					break;
				case NkVpAction::ModalCancel:
					// Echap annule ce qui est en cours, dans l'ordre de priorite :
					// une modale d'abord, un outil de zone ensuite. Sans cet ordre,
					// armer un rectangle puis appuyer Echap annulerait la mauvaise
					// chose.
					if (inModal)
						nk3d::Viewport3DModalCancel();
					else if (st.zoneTool >= 0) {
						st.zoneTool = -1;
						st.zoneActive = false;
					}
					break;
				case NkVpAction::ZoneRect:
					st.zoneTool = (st.zoneTool == 0) ? -1 : 0;
					st.zoneActive = false;
					break;
				case NkVpAction::ZoneCircle:
					st.zoneTool = (st.zoneTool == 2) ? -1 : 2;
					st.zoneActive = false;
					break;
				case NkVpAction::ToggleXray:
					st.xray = !st.xray;
					nk3d::Viewport3DSetXray(st.xray);
					break;
				// Les operations n'ont de sens QU'EN EDITION. Les laisser passer en
				// mode objet donnerait des commandes sans effet, donc un journal
				// d'annulation qui se remplit de riens.
				case NkVpAction::Extrude:
					if (edit && nk3d::Viewport3DExtrude(false))
						st.dirty = true;
					break;
				case NkVpAction::ExtrudeIndividual:
					if (edit && nk3d::Viewport3DExtrude(true))
						st.dirty = true;
					break;
				case NkVpAction::Delete:
					// Supprime CE QUE le mode designe : les faces selectionnees en
					// edition, l'objet actif en mode objet.
					if (edit) {
						if (nk3d::Viewport3DDeleteSelection())
							st.dirty = true;
					} else {
						const int32 act = nk3d::Viewport3DActiveObject();
						if (act >= 0) {
							nk3d::Viewport3DDeleteObject(act);
							st.dirty = true;
						}
					}
					break;
				case NkVpAction::Dissolve:
					if (edit && nk3d::Viewport3DDissolve())
						st.dirty = true;
					break;
				case NkVpAction::Merge:
					if (edit && nk3d::Viewport3DMerge(0)) // 0 = au centre
						st.dirty = true;
					break;
				case NkVpAction::MakeFace:
					if (edit && nk3d::Viewport3DMakeFace())
						st.dirty = true;
					break;
				case NkVpAction::Subdivide:
					if (edit && nk3d::Viewport3DSubdivide(1))
						st.dirty = true;
					break;
				case NkVpAction::LoopCut:
					if (edit && nk3d::Viewport3DLoopCut(1))
						st.dirty = true;
					break;
				case NkVpAction::Inset:
					// Epaisseur AUTOMATIQUE, proportionnelle a l'objet : une valeur
					// fixe donne un inset invisible sur un grand modele et un inset
					// qui traverse tout sur un petit.
					if (edit && nk3d::Viewport3DInset(0.1f, 0.f))
						st.dirty = true;
					break;
				case NkVpAction::BevelEdge:
					if (edit && nk3d::Viewport3DBevel(0.1f, 2, false))
						st.dirty = true;
					break;
				case NkVpAction::BevelVertex:
					if (edit && nk3d::Viewport3DBevel(0.1f, 2, true))
						st.dirty = true;
					break;
				case NkVpAction::Undo:
					nk3d::Viewport3DUndo();
					break;
				case NkVpAction::Redo:
					nk3d::Viewport3DRedo();
					break;
				// ── Vues ────────────────────────────────────────────────────
				case NkVpAction::ViewFront:
					nk3d::Viewport3DAxisView(0, false);
					break;
				case NkVpAction::ViewBack:
					nk3d::Viewport3DAxisView(0, true);
					break;
				case NkVpAction::ViewRight:
					nk3d::Viewport3DAxisView(1, false);
					break;
				case NkVpAction::ViewLeft:
					nk3d::Viewport3DAxisView(1, true);
					break;
				case NkVpAction::ViewTop:
					nk3d::Viewport3DAxisView(2, false);
					break;
				case NkVpAction::ViewBottom:
					nk3d::Viewport3DAxisView(2, true);
					break;
				case NkVpAction::ToggleOrtho:
					st.projection = (st.projection == 1) ? 0 : 1;
					break;
				case NkVpAction::FrameAll:
					nk3d::Viewport3DFrameAll();
					break;
				default:
					break;
			}
		}

		const NkTheme &theme = themes.Current();
		NkModelerPainter p(ui.dl, font, theme, roles, icons);

		// Fond general : il se voit dans les interstices entre panneaux, et c'est
		// ce qui donne la profondeur a trois niveaux de UI_SPEC 10bis.1.
		p.Fill({0.f, 0.f, W, H}, NkRole::WindowBg);

		PaintMenuBarI(p, lay.menu, "MonProjet", st, hit);
		PaintTabsI(p, lay.tabs, st, hit, ws, ui.input);
		PaintToolbar(p, lay.tool, st, hit, ws, combo);
		// Un panneau MASQUE n'est pas peint en taille nulle : il n'est pas peint du
		// tout. Le peindre dans un rectangle de 14 px declarerait ses zones cliquables
		// les unes sur les autres et un clic sur la poignee tomberait sur la premiere
		// ligne de la liste.
		if (st.showLeft)
			PaintHierarchy(p, lay.left, st, hit, ws, ui.input);
		PaintViewport(p, lay.view, st, hit, ws, combo, checks, shortcuts);
		if (st.showRight) {
			// PANNEAU DROIT UNIQUE (demande de Rihen) : Objet / Scene / Outil.
			// Proprietes et Details disaient deux fois la meme chose ; leurs
			// deux rectangles sont reunis en un seul.
			{
				NkRect rightR = lay.propsR;
				rightR.h = (lay.detailsR.y + lay.detailsR.h) - lay.propsR.y;
				PaintPropertiesUnified(p, rightR, st, hit, ws, ui.input, combo);
			}
		}
		if (st.showBrowser)
			PaintBrowser(p, lay.browser, st, hit, ws, ui.input);
		PaintStatus(p, lay.status, st);

		// Poignees de reouverture, a la place exacte qu'occupait le panneau.
		PaintPanelHandle(p, lay.handleLeft, hit, "handle.left", st.showLeft, NkIcon::ChevronRight);
		PaintPanelHandle(p, lay.handleRight, hit, "handle.right", st.showRight, NkIcon::ChevronLeft);
		PaintPanelHandle(p, lay.handleBrowser, hit, "handle.browser", st.showBrowser,
						 NkIcon::ChevronUp);

		// L'ORDRE DE CES TROIS APPELS EST SIGNIFIANT. Les separateurs doivent
		// recevoir le clic avant les panneaux qu'ils bordent ; le menu deroule
		// recouvre tout ; la boite de confirmation recouvre le menu. Le registre
		// donnant la priorite a la DERNIERE zone declaree, l'ordre de peinture EST
		// l'ordre de priorite -- il n'y a rien d'autre a synchroniser.
		// La liste deroulee est peinte AVANT les separateurs et le menu : elle doit
		// les recouvrir, et le registre donne la priorite a la derniere zone.
		PaintSplitters(p, lay, W, H, st, hit);
		// Le panneau des matcaps par-dessus TOUT : il peut etre ouvert depuis
		// la barre de la vue comme depuis le panneau Proprietes.
		PaintMatcapPopup(p, hit, st);
		DrawComboPopup(p, hit, ws, combo);
		DrawCheckPopup(p, hit, ws, checks);
		PaintModifierMenu(p, st, hit, ws, W, H);
		PaintAddObjectMenu(p, st, hit, ws, W, H);
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
			// TIRER UNE FENETRE MAXIMISEE LA RESTAURE, puis la deplace. C'est le
			// comportement de toutes les fenetres du systeme, et le refuser -- ce que
			// faisait la version precedente -- donne une barre de titre morte une fois
			// sur deux sans rien qui l'explique.
			//
			// On replace la fenetre restauree SOUS LE CURSEUR avant de rendre la main
			// a l'OS : sans cela elle saute en haut a gauche et la suite du geste
			// l'emmene ailleurs que la ou on croyait l'avoir attrapee. On conserve la
			// fraction horizontale du point saisi -- attraper la barre a droite doit
			// laisser le curseur a droite de la fenetre restauree.
			if (window.IsMaximized()) {
				const math::NkVec2 m = ui.input.mousePos;
				window.Restore();
				const math::NkVec2u sz = window.GetSize();
				const int32 nx = (int32)(m.x - (float32)sz.x * st.dragFracX);
				const int32 ny = (int32)(m.y - 12.f); // le curseur reste dans la barre
				window.SetPosition(nx < 0 ? 0 : nx, ny < 0 ? 0 : ny);
				st.maximized = false;
			}
			window.BeginDragMove();
		}
	}

	demo::Demo3DHostShutdown();
	nk3d::Viewport3DShutdown();
	renderer.Shutdown();
	ui.Shutdown();
	return 0;
}
