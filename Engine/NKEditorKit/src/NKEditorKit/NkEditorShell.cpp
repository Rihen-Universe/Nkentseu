// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkEditorShell.cpp — implementation de la coquille d'editeur (sur NKGui).
//   events -> BeginFrame -> menubar -> DockSpace -> panneaux -> palette -> rendu.
// =============================================================================
#include "NKEditorKit/NkEditorShell.h"
#include "NKEditorKit/NkThemeToGui.h" // LA conversion NkTheme -> NkGuiTheme (une seule)
#include "NKEditorKit/NkEditorCanvasRenderer.h" // backend de rendu par defaut (IDE)
#include "NKEditorKit/NkEditorTooltip.h"		// NkTooltip : infobulle des voyants du footer
#include <cstdio>								// snprintf (indicateur de zoom barre d'etat)

#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKMemory/NkAllocator.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKPlatform/NkEnv.h" // env::GetEnvVar (emplacements du sélecteur)
#include "NKMath/NkFunctions.h" // NkSin/NkCos/NkSqrt/NkAtan2 — la marque Rihen
                               // est TRACEE, pas chargee. ⚠️ `nkentseu::math`,
                               // pas `std::` (règle de Rodolf du 18/08).
#if defined(_WIN32)
#include <windows.h> // GetLogicalDrives (barre latérale disques)
#endif

using namespace nkentseu;
using namespace nkentseu::nkgui;
using namespace nkentseu::renderer;

namespace nkentseu {
	namespace editorkit {

		namespace {
			void CopyStr(char *dst, const char *src, usize cap) noexcept {
				if (!dst || cap == 0)
					return;
				usize i = 0;
				if (src) {
					for (; src[i] && i + 1 < cap; ++i)
						dst[i] = src[i];
				}
				dst[i] = '\0';
			}

			bool StartsWith(const char *s, const char *pre) noexcept {
				for (; *pre; ++s, ++pre)
					if (*s != *pre)
						return false;
				return true;
			}

			bool StrEqual(const char *a, const char *b) noexcept {
				while (*a && *b) {
					if (*a != *b)
						return false;
					++a;
					++b;
				}
				return *a == *b;
			}

			constexpr NkColor kPaletteBg = {30, 33, 42, 250};
			constexpr NkColor kPaletteBorder = {90, 150, 230, 255};
			constexpr NkColor kPaletteSel = {64, 110, 200, 235};
			constexpr NkColor kTextPrimary = {235, 236, 242, 255};
			constexpr NkColor kTextSecondary = {180, 185, 196, 255};
			constexpr NkColor kTextTertiary = {130, 135, 148, 255};
			constexpr NkColor kBackdrop = {0, 0, 0, 130};

			NkWindow::NkCursorType MapCursor(NkGuiCursor c) noexcept {
				switch (c) {
					case NkGuiCursor::Text:
						return NkWindow::NkCursorType::TextInput;
					case NkGuiCursor::Hand:
						return NkWindow::NkCursorType::Hand;
					case NkGuiCursor::ResizeEW:
						return NkWindow::NkCursorType::ResizeWE;
					case NkGuiCursor::ResizeNS:
						return NkWindow::NkCursorType::ResizeNS;
					default:
						return NkWindow::NkCursorType::Arrow;
				}
			}

			int32 SideToZone(NkEditorDockSide s) noexcept {
				switch (s) {
					case NkEditorDockSide::NK_LEFT:
						return 1;
					case NkEditorDockSide::NK_RIGHT:
						return 2;
					case NkEditorDockSide::NK_TOP:
						return 3;
					case NkEditorDockSide::NK_BOTTOM:
						return 4;
					case NkEditorDockSide::NK_CENTER:
					default:
						return 0;
				}
			}
		} // namespace

		// ── NkEditorPanel : constructeur (defini dans la lib) ────────────────────
		NkEditorPanel::NkEditorPanel(const char *title, NkEditorDockSide defaultSide) noexcept {
			CopyStr(mTitle, title ? title : "Panel", sizeof(mTitle));
			mDefaultSide = defaultSide;
		}

		// ── Cycle de vie ─────────────────────────────────────────────────────────
		NkEditorShell::~NkEditorShell() {
			// Libere les atlas du cache de code par taille (alloues a la demande).
			for (int32 i = 0; i < kCodeCacheN; ++i)
				if (mCodeSlots[i].font) {
					memory::NkGetDefaultAllocator().Delete(mCodeSlots[i].font);
					mCodeSlots[i].font = nullptr;
				}
			if (mRenderer) { // libere le contexte GPU avant la fenetre
				mRenderer->Shutdown();
				if (mOwnsRenderer)
					memory::NkGetDefaultAllocator().Delete(mRenderer);
				mRenderer = nullptr;
			}
			mWindow.Close();
		}

		bool NkEditorShell::Init(const NkEditorShellConfig &config) noexcept {
			mGraphicsApi = config.graphicsApi;

			NkWindowConfig wc;
			wc.title = config.title;
			wc.width = config.width;
			wc.height = config.height;
			wc.minWidth = 1024; // taille MINI de l'IDE (sidebar + panneau lisibles)
			wc.minHeight = 640;
			wc.centered = true;
			wc.resizable = config.resizable;
			wc.frame = false; // SANS bordure OS -> barre de titre custom (VSCode)
			if (!mWindow.Create(wc))
				return false;
			CopyStr(mTitle, config.title, sizeof(mTitle));

			// Backend de rendu : injecte (app NKRHI/NKRenderer) ou NKCanvas par
			// defaut (IDE). Resolution AUTO->API faite par l'impl elle-meme.
			if (config.renderer) {
				mRenderer = config.renderer;
				mOwnsRenderer = false;
			} else {
				mRenderer = memory::NkGetDefaultAllocator().New<NkEditorCanvasRenderer>();
				mOwnsRenderer = true;
			}
			if (!mRenderer || !mRenderer->Init(mWindow, mGraphicsApi)) {
				if (mRenderer && mOwnsRenderer)
					memory::NkGetDefaultAllocator().Delete(mRenderer);
				mRenderer = nullptr;
				return false;
			}

			mUI.Init(static_cast<int32>(config.width), static_cast<int32>(config.height));

			// Theme GitHub Dark (palette fournie). #0D1117 fond, #191D23 surfaces,
			// #010409 chrome sombre, #1F6FEB accent, #DFDFDF texte, #519ABA secondaire.
			//
			// ⚠️ POURQUOI CES SEIZE COULEURS NE PASSENT PAS PAR `NkThemeVersGui`,
			//    ET LA MESURE QUI L A DECIDE (2026-08-29). L intention etait de les
			//    remplacer par une conversion -- « une recopie manuelle est une
			//    conversion ecrite sans etre nommee, elle divergera au premier role
			//    ajoute ». Le raisonnement est juste. **La mesure dit que la
			//    divergence a DEJA eu lieu, et qu elle porte sur le MAPPAGE, pas sur
			//    la palette** : ces seize valeurs ne sont pas exprimables dans le
			//    vocabulaire des roles. Sept champs le prouvent :
			//
			//      champ         ici               ce que la conversion donnerait
			//      button        #191D23           InputBg  -> #0D1117
			//      track         #0D1117           InputBg  -> #0D1117
			//                    ^ `button` et `track` DIFFERENT ici, et un seul
			//                      role `InputBg` ne peut pas rendre deux couleurs.
			//      tabBar        #191D23           WindowBg -> #0D1117
			//      tabActive     #0D1117           PanelBg  -> #010409
			//      buttonHover   #212730           Mix(button, accent, 0.20)
			//      tabHover      #212730           Mix(tab, accent, 0.20)
			//      selection     #1F6FEB **a=200** accent, a=255
			//      rounding      0.f               non touche -> reste a 5.f
			//
			//    Convertir changerait donc l APPARENCE, pas seulement la forme du
			//    code. ⚠️ Et ca ne toucherait pas que cette coquille : **seul
			//    NkUIDesign appelle `ApplyTheme`** -- verifie par recherche sur
			//    l arbre. Pour toutes les autres applications (NKCode en tete, en
			//    pause depuis des semaines et qui fonctionne), cette palette EST le
			//    theme livre. Un nettoyage de code n a pas a restyler quatre
			//    editeurs en passant.
			//
			// NOTE : CE QUE CA DIT VRAIMENT, et c est plus utile que le refactor --
			//    `NkTheme` n a pas les roles qu il faudrait pour decrire cette
			//    palette : il lui manque de quoi distinguer le fond d un BOUTON du
			//    fond d un CHAMP, et de quoi dire la barre d onglets. C est un
			//    manque du VOCABULAIRE DES ROLES, nomme ici et non resolu :
			//    l ajouter est une decision de conception du kit, pas un effet de
			//    bord d une extraction faite a cote.
			//
			// == CE QUI RESTE A FAIRE, ET IL NE MANQUE PLUS QUE LE JOUR ==========
			//    ⚠️ LES DEUX ROLES MANQUANTS EXISTENT DEPUIS LE 2026-08-29 :
			//    `NkRole::ButtonBg` et `NkRole::TabBarBg` (decision de Rodolf via le
			//    canal, sur la mesure ci-dessus). Ils se replient sur `InputBg` et
			//    `WindowBg` tant qu'un theme ne les pose pas -- mesure appariee : les
			//    4 themes integres x 35 champs sortent OCTET POUR OCTET identiques
			//    avant et apres leur ajout. **La palette ci-dessous est donc
			//    desormais EXPRIMABLE.**
			//
			//    Ce qui reste, quand quelqu un le fera de jour et avec Rodolf :
			//      1. ecrire cette palette comme un `NkTheme` nomme -- appelons-le
			//         `NkThemeCoquilleDefaut()` -- avec `ButtonBg = #191D23`,
			//         `TabBarBg = #191D23`, et les autres roles tels que la table
			//         ci-dessus les donne ;
			//      2. remplacer les seize lignes qui suivent par un seul
			//         `NkThemeVersGui(mUI.theme, NkThemeCoquilleDefaut())` ;
			//      3. reposer A LA MAIN, APRES l'appel, les trois valeurs que la
			//         conversion ne porte pas et ne doit pas porter :
			//             mUI.theme.rounding = 0.f;        // geometrie, pas couleur
			//             mUI.theme.selection.a = 200;     // alpha, aucun role
			//             mUI.theme.tabActive = ...;       // cf. la table ci-dessus
			//         ⚠️ Ces trois-la sont le RESTE IRREDUCTIBLE. Les faire entrer
			//            dans les roles serait inventer du vocabulaire pour cacher
			//            une exception -- ce qui est exactement le defaut qu'on
			//            vient de corriger, a l'envers.
			//      4. relancer une mesure appariee de la meme forme que celle du
			//         29/08 (4 themes x 35 champs, diff attendu VIDE) AVANT de
			//         croire que le rendu n'a pas bouge. Une sonde de trente lignes
			//         suffit ; elle a ete ecrite, utilisee, puis retiree ce jour-la.
			//
			//    ⚠️ POURQUOI CE N'EST PAS FAIT ICI. Seul NkUIDesign appelle
			//       `ApplyTheme` : pour NKCode et les autres, cette palette EST le
			//       theme livre. La migration se VOIT a l'ecran, donc elle se valide
			//       a l'oeil -- et ce n'est pas un travail de nuit.
			// == ETAPES 2 ET 3 DE LA MIGRATION (2026-08-30) =====================
			// Les seize recopies manuelles sont devenues UN appel a LA conversion,
			// sur le theme nomme `NkThemeCoquilleDefaut()`. Les quatre etapes
			// ecrites ci-dessus ont ete suivies dans l ordre ; la mesure du champ
			// par champ est dans le message du commit de migration.
			NkThemeVersGui(mUI.theme, NkThemeCoquilleDefaut());
			// -- LE RESTE IRREDUCTIBLE, repose A LA MAIN, comme documente --------
			// Ces trois valeurs n ont pas de role et ne doivent pas en avoir un
			// (cf. la note ci-dessus : inventer du vocabulaire pour cacher une
			// exception serait le defaut corrige, a l envers).
			NkGuiTheme &t = mUI.theme;
			t.selection.a = 200;			 // le voile de selection reste un VOILE
			t.tabActive = {13, 17, 23, 255}; // onglet actif = fond editeur #0D1117
			t.rounding = 0.f;				 // coins droits

			mDefaultTheme = mUI.theme;	 // theme par defaut (vit dans l'app) -> 'Reinitialiser'
			NkLoadTheme(mUI.theme);		 // applique le theme utilisateur sauvegarde s'il existe
			mDefaultSyntax = mUI.syntax; // couleurs langages par defaut
			NkLoadSyntax(mUI.syntax);	 // applique les couleurs langages sauvegardees

			// Un nœud de dock à 1 seul panneau n'affiche PAS de barre d'onglets
			// (l'éditeur ne montre que ses onglets de fichiers ; pas de tab "Editeur").
			mUI.dockHideSingleTab = true;

			// Presse-papiers : relie le contexte NKGui a la fenetre OS (NKWindow).
			mUI.clipboardUser = &mWindow;
			mUI.clipboardGetFn = [](void *u, NkString &out) { out = static_cast<NkWindow *>(u)->GetClipboardText(); };
			mUI.clipboardSetFn = [](void *u, const char *t) {
				static_cast<NkWindow *>(u)->SetClipboardText(NkString(t));
			};

			// Hook barre d'onglets : le panneau ACTIF dessine ses actions a droite.
			mUI.dockHeaderUser = this;
			mUI.dockHeaderFn = [](NkGuiContext &c, const NkRect &bar, NkGuiId win, void *u) {
				auto *self = static_cast<NkEditorShell *>(u);
				// Region HAUT|BAS -> le SHELL reserve un espace a DROITE pour maximiser/replier
				// (dispo pour TOUS les onglets de la region, pas seulement l'actif).
				const bool region = self->IsBottomRegionTab(c, win);
				const float32 rw = region ? 2.f * bar.h + 6.f : 0.f;
				const NkRect innerBar = {bar.x, bar.y, bar.w - rw, bar.h};
				for (int32 i = 0; i < self->mNumPanels; ++i)
					if (c.GetId(self->mPanels[i]->Title()) == win) {
						// Le masquage anti clic-a-travers (souris sur un popup) vise les corps en
						// hit-test brut ; les ACTIONS de barre d onglets (vrais widgets NKGui, dont
						// le combo de shells et SON popup) recoivent l input REEL, sinon le menu
						// deroulant ouvert par le panneau est injouable a la souris.
						if (self->mPopupMasked) {
							const nkgui::NkGuiInput masked = c.input;
							c.input = self->mRealInput;
							self->mPanels[i]->OnTabBarActions(c, innerBar);
							c.input = masked;
						} else
							self->mPanels[i]->OnTabBarActions(c, innerBar);
						break;
					}
				if (region) { // boutons region (shell-level), a l'extreme droite
					const NkRect rb = {bar.x + bar.w - rw + 6.f, bar.y, rw - 6.f, bar.h};
					if (self->mPopupMasked) {
						const nkgui::NkGuiInput masked = c.input;
						c.input = self->mRealInput;
						self->DrawRegionButtons(c, rb, win);
						c.input = masked;
					} else
						self->DrawRegionButtons(c, rb, win);
				}
			};

			// ── DEUX polices distinctes (comme VSCode), pilotees par les reglages ──
			// INTERFACE (proportionnelle, defaut Inter) + CODE/TERMINAL (monospace,
			// defaut DejaVu Sans Mono). Atlas SEPARES => texId distincts. Le choix est
			// lu depuis le fichier de config (modifiable par l'utilisateur a chaud).
			mCodeFont.texId = mFont.TexId() + 1u; // atlas distinct (anti-collision backend)
			NkLoadFontPrefs(mFontPrefs);
			LoadFontsFromPrefs();

			mUI.windowDockingEnabled = true; // fusion de fenetres flottantes (opt-in)

			mLastWidth = config.width;
			mLastHeight = config.height;

			// Acces aux Preferences via la palette (Ctrl+P) — fiable quel que soit le menu.
			RegisterCommand(
				"Preferences : Polices", +[](void *u) { static_cast<NkEditorShell *>(u)->OpenPreferences(0); }, this);
			RegisterCommand(
				"Preferences : Theme", +[](void *u) { static_cast<NkEditorShell *>(u)->OpenPreferences(1); }, this);
			RegisterCommand(
				"Preferences : Langages", +[](void *u) { static_cast<NkEditorShell *>(u)->OpenPreferences(2); }, this);

			HookEvents();
			return true;
		}

		void NkEditorShell::HookEvents() noexcept {
			auto &events = NkEvents();

			// Croix de la barre de titre = fermeture EXPLICITE de cette fenetre. On
			// previent l'application AVANT de sortir de la boucle : elle seule sait
			// quoi en faire (NKCode s'en sert pour ne PAS restaurer cette fenetre au
			// lancement suivant). Ctrl+Q passe par RequestClose() et ne declenche
			// donc pas ce rappel — la distinction est voulue.
			events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent *) {
				// Le rappel peut VETOER : false = l'application a pris la main (par
				// exemple pour demander confirmation quand des fichiers sont modifies)
				// et fermera elle-meme via RequestClose() le moment venu. Fermer ici
				// sans attendre contournait toute confirmation.
				RequestQuit(); // meme chemin que la croix dessinee et le menu Quitter
			});
			// Drop de FICHIERS depuis l'OS (Explorateur Windows…) -> handler de l'app.
			events.AddEventCallback<NkDropFileEvent>([this](NkDropFileEvent *e) {
				if (mDropFn && e)
					mDropFn(mDropUser, e->data.paths, e->data.x, e->data.y);
			});
			events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent *e) {
				mUI.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
			});
			events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent *e) {
				if (e->GetButton() == NkMouseButton::NK_MB_LEFT)
					mUI.input.mouseDown[0] = true;
				if (e->GetButton() == NkMouseButton::NK_MB_RIGHT)
					mUI.input.mouseDown[1] = true;
				if (e->GetButton() == NkMouseButton::NK_MB_MIDDLE)
					mUI.input.mouseDown[2] = true; // convention [0]=G [1]=D [2]=milieu
				mUI.input.ctrlDown = e->GetModifiers().ctrl;
				mUI.input.shiftDown = e->GetModifiers().shift;
				mUI.input.altDown = e->GetModifiers().alt;
			});
			events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent *e) {
				if (e->GetButton() == NkMouseButton::NK_MB_LEFT)
					mUI.input.mouseDown[0] = false;
				if (e->GetButton() == NkMouseButton::NK_MB_RIGHT)
					mUI.input.mouseDown[1] = false;
				if (e->GetButton() == NkMouseButton::NK_MB_MIDDLE)
					mUI.input.mouseDown[2] = false;
			});
			// Molette : scroll vertical + horizontal (consommee en EndFrame par NKGui).
			events.AddEventCallback<NkMouseWheelVerticalEvent>([this](NkMouseWheelVerticalEvent *e) {
				mUI.input.wheel += static_cast<float32>(e->GetDeltaY());
				const auto m = e->GetModifiers();
				mUI.input.ctrlDown = m.ctrl;
				mUI.input.shiftDown = m.shift;
				mUI.input.altDown = m.alt;
			});
			events.AddEventCallback<NkMouseWheelHorizontalEvent>(
				[this](NkMouseWheelHorizontalEvent *e) { mUI.input.wheelH += static_cast<float32>(e->GetDeltaX()); });
			// Double-clic OS (evenement dedie) : injecte pour la selection de mot.
			events.AddEventCallback<NkMouseDoubleClickEvent>([this](NkMouseDoubleClickEvent *e) {
				mUI.input.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
				if (e->GetButton() == NkMouseButton::NK_MB_LEFT)
					mUI.input.SetDoubleClick(0);
			});
			// Saisie texte (codepoints) -> file de caracteres NKGui.
			events.AddEventCallback<NkTextInputEvent>(
				[this](NkTextInputEvent *e) { mUI.input.PushChar(e->GetCodepoint()); });
			events.AddEventCallback<NkKeyPressEvent>([this](NkKeyPressEvent *e) {
				const NkKey k = e->GetKey();
				MapEditKey(k, true);
				mUI.input.ctrlDown = e->GetModifiers().ctrl;
				mUI.input.shiftDown = e->GetModifiers().shift;
				mUI.input.altDown = e->GetModifiers().alt;
				if (e->GetModifiers().ctrl) { // raccourcis copier/couper/coller/tout-selectionner
					if (k == NkKey::NK_C)
						mUI.input.wantCopy = true;
					else if (k == NkKey::NK_X)
						mUI.input.wantCut = true;
					else if (k == NkKey::NK_V)
						mUI.input.wantPaste = true;
					else if (k == NkKey::NK_A)
						mUI.input.wantSelectAll = true;
					// Zoom éditeur au CLAVIER : Ctrl+= / Ctrl++ (pavé) zoome, Ctrl+- / Ctrl+pavé- dézoome,
					// Ctrl+0 réinitialise. Traité ICI (fiable, comme les autres Ctrl+touche) plutôt que
					// via KeyPressedRepeat dans un panneau, qui ne déclenchait pas.
					else if (k == NkKey::NK_EQUALS || k == NkKey::NK_NUMPAD_ADD)
						NudgeCodeFontSize(1.f);
					else if (k == NkKey::NK_MINUS || k == NkKey::NK_NUMPAD_SUB)
						NudgeCodeFontSize(-1.f);
					else if (k == NkKey::NK_NUM0 || k == NkKey::NK_NUMPAD_0)
						ResetCodeFontSize();
				}
				if (k == NkKey::NK_P && e->GetModifiers().ctrl) {
					mPaletteOpen = !mPaletteOpen;
					mPaletteSel = 0;
					return;
				}
				if (mPaletteOpen) {
					if (k == NkKey::NK_ESCAPE)
						mPaletteOpen = false;
					else if (k == NkKey::NK_DOWN && mNumCommands > 0)
						mPaletteSel = (mPaletteSel + 1) % mNumCommands;
					else if (k == NkKey::NK_UP && mNumCommands > 0)
						mPaletteSel = (mPaletteSel - 1 + mNumCommands) % mNumCommands;
					else if (k == NkKey::NK_ENTER) {
						ExecuteCommand(mPaletteSel);
						mPaletteOpen = false;
					}
					return;
				}
				// Raccourcis Ctrl+<lettre> (ex. Ctrl+S, Ctrl+B) meme pendant la frappe.
				if (e->GetModifiers().ctrl)
					TryRunShortcut(k, e->GetModifiers().shift);
			});
			events.AddEventCallback<NkKeyReleaseEvent>([this](NkKeyReleaseEvent *e) {
				MapEditKey(e->GetKey(), false);
				mUI.input.ctrlDown = e->GetModifiers().ctrl;
				mUI.input.shiftDown = e->GetModifiers().shift;
				mUI.input.altDown = e->GetModifiers().alt;
			});
		}

		// Mappe une touche OS d'edition vers l'etat ENFONCE NKGui (press/release ->
		// repetition au maintien geree par NKGui). Sans ce pont, aucune navigation
		// clavier ni edition de texte dans les panneaux.
		void NkEditorShell::MapEditKey(NkKey k, bool down) noexcept {
			switch (k) {
				case NkKey::NK_LEFT:
					mUI.input.SetKey(NkGuiKey::Left, down);
					break;
				case NkKey::NK_RIGHT:
					mUI.input.SetKey(NkGuiKey::Right, down);
					break;
				case NkKey::NK_UP:
					mUI.input.SetKey(NkGuiKey::Up, down);
					break;
				case NkKey::NK_DOWN:
					mUI.input.SetKey(NkGuiKey::Down, down);
					break;
				case NkKey::NK_HOME:
					mUI.input.SetKey(NkGuiKey::Home, down);
					break;
				case NkKey::NK_END:
					mUI.input.SetKey(NkGuiKey::End, down);
					break;
				case NkKey::NK_BACK:
					mUI.input.SetKey(NkGuiKey::Backspace, down);
					break;
				case NkKey::NK_DELETE:
					mUI.input.SetKey(NkGuiKey::Delete, down);
					break;
				case NkKey::NK_ENTER:
					mUI.input.SetKey(NkGuiKey::Enter, down);
					break;
				case NkKey::NK_ESCAPE:
					mUI.input.SetKey(NkGuiKey::Escape, down);
					break;
				// Touches additionnelles pour les raccourcis du file browser.
				case NkKey::NK_TAB:
					mUI.input.SetKey(NkGuiKey::Tab, down);
					break;
				case NkKey::NK_F2:
					mUI.input.SetKey(NkGuiKey::F2, down);
					break;
				case NkKey::NK_F5:
					mUI.input.SetKey(NkGuiKey::F5, down);
					break;
				case NkKey::NK_C:
					mUI.input.SetKey(NkGuiKey::C, down);
					break;
				case NkKey::NK_D:
					mUI.input.SetKey(NkGuiKey::D, down);
					break;
				case NkKey::NK_F:
					mUI.input.SetKey(NkGuiKey::F, down);
					break; // Ctrl+F recherche
				case NkKey::NK_SPACE:
					mUI.input.SetKey(NkGuiKey::Space, down);
					break; // Ctrl+Espace autocompletion
				case NkKey::NK_F8:
					mUI.input.SetKey(NkGuiKey::F8, down);
					break;
				case NkKey::NK_F12:
					mUI.input.SetKey(NkGuiKey::F12, down);
					break;
				case NkKey::NK_J:
					mUI.input.SetKey(NkGuiKey::J, down);
					break;
				case NkKey::NK_W:
					mUI.input.SetKey(NkGuiKey::W, down);
					break;
				case NkKey::NK_T:
					mUI.input.SetKey(NkGuiKey::T, down);
					break;
				case NkKey::NK_I:
					mUI.input.SetKey(NkGuiKey::I, down);
					break;
				case NkKey::NK_O:
					mUI.input.SetKey(NkGuiKey::O, down);
					break;
				case NkKey::NK_BACKSLASH:
					mUI.input.SetKey(NkGuiKey::Backslash, down);
					break;
				case NkKey::NK_PERIOD:
					mUI.input.SetKey(NkGuiKey::Period, down);
					break;
				case NkKey::NK_H:
					mUI.input.SetKey(NkGuiKey::H, down);
					break;
				case NkKey::NK_L:
					mUI.input.SetKey(NkGuiKey::L, down);
					break;
				case NkKey::NK_N:
					mUI.input.SetKey(NkGuiKey::N, down);
					break;
				case NkKey::NK_G:
					mUI.input.SetKey(NkGuiKey::G, down);
					break;
				case NkKey::NK_K:
					mUI.input.SetKey(NkGuiKey::K, down);
					break;
				case NkKey::NK_SLASH:
					mUI.input.SetKey(NkGuiKey::Slash, down);
					break;
				case NkKey::NK_LBRACKET:
					mUI.input.SetKey(NkGuiKey::LBracket, down);
					break;
				case NkKey::NK_RBRACKET:
					mUI.input.SetKey(NkGuiKey::RBracket, down);
					break;
				case NkKey::NK_Z:
					mUI.input.SetKey(NkGuiKey::Z, down);
					break;
				case NkKey::NK_Y:
					mUI.input.SetKey(NkGuiKey::Y, down);
					break;
				case NkKey::NK_NUM0:
					mUI.input.SetKey(NkGuiKey::Num0, down);
					break;
				case NkKey::NK_NUMPAD_0: // pavé numérique (AZERTY : le 0 du haut = Maj+À)
					mUI.input.SetKey(NkGuiKey::Num0, down);
					break;
				case NkKey::NK_NUM1:
					mUI.input.SetKey(NkGuiKey::Num1, down);
					break;
				case NkKey::NK_NUM2:
					mUI.input.SetKey(NkGuiKey::Num2, down);
					break;
				case NkKey::NK_MINUS:
					mUI.input.SetKey(NkGuiKey::Minus, down);
					break; // Ctrl+- : dézoom éditeur
				case NkKey::NK_EQUALS:
					mUI.input.SetKey(NkGuiKey::Equal, down);
					break; // Ctrl+= / Ctrl++ : zoom éditeur
				// ══════════════════════════════════════════════════════════════
				// 🔴 CE QUI MANQUAIT ICI, ET QUI ETAIT ACCUSE AILLEURS (2026-09-02)
				// ══════════════════════════════════════════════════════════════
				// `NkGuiTypes.h` portait DEUX signalements disant que `Ctrl+1..6`
				// et `Ctrl+,` etaient « annonces par l'ecran Parametres sans
				// qu'aucun code puisse les recevoir, FAUTE DE CODE DE TOUCHE ».
				// **Le diagnostic etait faux.** `Num3`..`Num6` et `Comma` sont
				// dans l'enumeration depuis le lot du launcher : c'est CE
				// `switch`-ci qui s'arretait a `NK_NUM2` et ne les emettait
				// jamais.
				//
				// *Une valeur d'enumeration que personne n'emet est aussi morte
				// qu'une valeur absente -- et elle est PIRE, parce qu'elle a l'air
				// presente.* On la lit dans l'enum, on en conclut que le socle
				// sait la recevoir, et on va chercher le defaut chez l'appelant.
				// Deux signalements successifs l'ont cherche du mauvais cote.
				//
				// ⚠️ ET LE PAVE NUMERIQUE SUIT LA MEME REGLE QUE `Num0`..`Num2`
				//    quelques lignes plus haut : sur AZERTY, les chiffres du haut
				//    demandent `Maj`, donc le pave est la seule saisie directe. Le
				//    traiter pour 0-2 et pas pour 3-6 aurait fait marcher la
				//    moitie des raccourcis sur la moitie des claviers.
				case NkKey::NK_NUM3:
				case NkKey::NK_NUMPAD_3:
					mUI.input.SetKey(NkGuiKey::Num3, down);
					break;
				case NkKey::NK_NUM4:
				case NkKey::NK_NUMPAD_4:
					mUI.input.SetKey(NkGuiKey::Num4, down);
					break;
				case NkKey::NK_NUM5:
				case NkKey::NK_NUMPAD_5:
					mUI.input.SetKey(NkGuiKey::Num5, down);
					break;
				case NkKey::NK_NUM6:
				case NkKey::NK_NUMPAD_6:
					mUI.input.SetKey(NkGuiKey::Num6, down);
					break;
				case NkKey::NK_COMMA:
					mUI.input.SetKey(NkGuiKey::Comma, down);
					break; // Ctrl+, : Parametres
				// ── LES HUIT LETTRES QUI COMPLETENT L'ALPHABET (A..Z) ─────────
				// Posees toutes d'un coup plutot que `A` et `R` seules : la
				// prochaine application qui voudra `S` ou `E` ne rouvrira pas ce
				// fichier. *Un chantier groupe qui se rouvre n'a pas ete fait.*
				case NkKey::NK_A:
					mUI.input.SetKey(NkGuiKey::A, down);
					break; // Ctrl+A : tout selectionner
				case NkKey::NK_B:
					mUI.input.SetKey(NkGuiKey::B, down);
					break;
				case NkKey::NK_E:
					mUI.input.SetKey(NkGuiKey::E, down);
					break;
				case NkKey::NK_M:
					mUI.input.SetKey(NkGuiKey::M, down);
					break;
				case NkKey::NK_Q:
					mUI.input.SetKey(NkGuiKey::Q, down);
					break;
				case NkKey::NK_R:
					mUI.input.SetKey(NkGuiKey::R, down);
					break; // R : outil rectangle
				case NkKey::NK_S:
					mUI.input.SetKey(NkGuiKey::S, down);
					break;
				case NkKey::NK_U:
					mUI.input.SetKey(NkGuiKey::U, down);
					break;
				default:
					break;
			}
		}

		// ── Enregistrement ───────────────────────────────────────────────────────
		bool NkEditorShell::AddPanel(NkEditorPanel *panel) noexcept {
			if (!panel || mNumPanels >= MAX_PANELS)
				return false;
			mPanels[mNumPanels++] = panel;
			return true;
		}

		bool NkEditorShell::RegisterCommand(const char *name, NkEditorCommandFn fn, void *user,
											const char *shortcut) noexcept {
			if (!name || !fn || mNumCommands >= MAX_COMMANDS)
				return false;
			NkEditorCommand &c = mCommands[mNumCommands++];
			CopyStr(c.name, name, sizeof(c.name));
			CopyStr(c.shortcut, shortcut ? shortcut : "", sizeof(c.shortcut));
			c.fn = fn;
			c.user = user;
			return true;
		}

		void NkEditorShell::ExecuteCommand(int32 index) noexcept {
			if (index < 0 || index >= mNumCommands)
				return;
			if (mCommands[index].fn)
				mCommands[index].fn(mCommands[index].user);
		}

		// ── Boucle principale ────────────────────────────────────────────────────
		// Callback OS appele pendant la boucle modale de resize/move (timer 0xB1A5) :
		// redessine UNE frame a la nouvelle taille -> supprime le "stretch" du framebuffer.
		void NkEditorShell::SizeMoveFrameThunk(void *user) noexcept {
			if (user)
				static_cast<NkEditorShell *>(user)->RenderFrame();
		}

		int NkEditorShell::Run() noexcept {
			NkEvents().SetSizeMoveFrameCallback(&SizeMoveFrameThunk, this); // anti-stretch pendant le resize natif
			while (mRunning && mWindow.IsOpen()) {
				while (NkEvent *ev = NkEvents().PollEvent()) {
					(void)ev;
				}
				if (!mRunning)
					break;
				RenderFrame();
				// Hand-off NATIF differe : BeginResize/BeginDragMove BLOQUENT (boucle modale
				// OS) ; on les lance ICI (hors frame) pour eviter la re-entrance de RenderFrame.
				// Pendant la boucle modale, le callback ci-dessus rappelle RenderFrame -> rendu live.
				if (mPendingDragMove) {
					mPendingDragMove = false;
					mWindow.BeginDragMove();
				} else if (mPendingResizeEdge >= 0) {
					const NkWindow::NkResizeEdge e = static_cast<NkWindow::NkResizeEdge>(mPendingResizeEdge);
					mPendingResizeEdge = -1;
					mWindow.BeginResize(e);
				}
			}
			return 0;
		}

		void NkEditorShell::RenderFrame() noexcept {
			float32 dt = mClock.Tick().delta;
			if (dt <= 0.f)
				dt = 1.f / 60.f;
			if (dt > 0.1f)
				dt = 0.1f;

			// Resize : suit la taille de la fenetre OS.
			const math::NkVec2u wsz = mWindow.GetSize();
			if (wsz.x > 0 && wsz.y > 0 && (wsz.x != mLastWidth || wsz.y != mLastHeight)) {
				mRenderer->OnResize(wsz.x, wsz.y);
				mLastWidth = wsz.x;
				mLastHeight = wsz.y;
			}
			// CACHE la géométrie CHAQUE frame (fenêtre encore valide) : SaveUiState/
			// SaveWindowGeom tournent APRÈS Run() quand la fenêtre peut être détruite
			// -> IsMaximized/GetSize renverraient un état faux. On sauve donc l'état
			// vu à la dernière frame. La taille FENÊTRÉE n'est mémorisée que hors max.
			mGeomMax = mWindow.IsMaximized();
			if (!mGeomMax) {
				const math::NkVec2u gp = mWindow.GetPosition();
				mGeomX = static_cast<int32>(gp.x);
				mGeomY = static_cast<int32>(gp.y);
				mGeomW = static_cast<int32>(wsz.x);
				mGeomH = static_cast<int32>(wsz.y);
			}
			mGeomValid = true;
			const math::NkVec2u sz = mRenderer->Size();
			if (sz.x > 0 && sz.y > 0) {
				mUI.viewW = static_cast<int32>(sz.x);
				mUI.viewH = static_cast<int32>(sz.y);
			}

			// Rechargement de police differe (zoom Ctrl+molette / Ctrl+±) : execute ICI,
			// avant BeginFrame, donc aucune draw list ne reference l'ancien atlas pendant
			// la re-rasterisation + re-upload backend.
			// Full = les deux polices (changement UI/prefs, immediat). Zoom = DEBOUNCE :
			// l'atlas code n'est reconstruit qu'apres kCodeReloadDebounce sans nouveau cran
			// -> molette fluide (pas de rebuild par cran), un seul rebuild a la fin.
			if (mFontReloadPending) {
				mFontReloadPending = false;
				mCodeReloadCountdown = mTermReloadCountdown = -1.f;
				LoadFontsFromPrefs();
			} else {
				if (mCodeReloadCountdown >= 0.f) {
					mCodeReloadCountdown -= dt;
					if (mCodeReloadCountdown <= 0.f) {
						mCodeReloadCountdown = -1.f;
						BuildCodeSlot(mCodePendingPx);
					}
				}
				if (mTermReloadCountdown >= 0.f) { // zoom du TERMINAL (survol) : meme debounce
					mTermReloadCountdown -= dt;
					if (mTermReloadCountdown <= 0.f) {
						mTermReloadCountdown = -1.f;
						LoadTermFont();
					}
				}
			}

			mUI.BeginFrame(dt);

			const float32 W = static_cast<float32>(mUI.viewW);
			const float32 H = static_cast<float32>(mUI.viewH);
			mUI.dl.AddRectFilled({0.f, 0.f, W, H}, mUI.theme.bgPrimary);

			NkEditorFrameContext ec;
			ec.ui = &mUI;
			ec.dt = dt;

			// FIX deadlock launcher->editeur : en fullScreen la barre de menus n'est PAS
			// dessinee (cf. BuildMenuBar, garde `if (!appFullScreen)`), donc mAppMenuFn (qui
			// pose appFullScreen = showStart) ne tournait plus une fois dans le launcher ->
			// appFullScreen restait bloque a true et l'editeur n'etait JAMAIS atteint apres
			// showStart=false. On resynchronise donc les flags AVANT de decider du plein-ecran.
			if (mAppMenuFn)					  // TOUJOURS avant la décision plein-écran : sinon la 1re frame rend
				mAppMenuFn(ec, mAppMenuUser); // l'IDE avant le launcher (flash au démarrage)

			// Ecran de demarrage (launcher) : occupe tout le corps, sans barre
			// d'outils / panneaux / barre d'etat (façon « page de demarrage » VS).
			const bool fullScreen = mUI.appFullScreen && mStartScreenFn;

			// ⚠️ COTES IMPOSEES PAR L APPLICATION quand elle en pose (SetHeaderLayout),
			//    sinon le calcul historique. Elles ne passent PAS par `S()` : une
			//    maquette qui dit 28 doit se mesurer 28 sur la capture.
			const float32 titleH =
				(mHeaderTitleH > 0.f) ? mHeaderTitleH : (mUI.ItemHeight() + mUI.S(10.f));
			mUI.titleBarH = titleH;								   // l'ecran de demarrage doit commencer en dessous
			const float32 bandH = (mHeaderBandH > 0.f) ? mHeaderBandH : mUI.S(46.f);
			const float32 toolbarH = (mToolbarFn && !fullScreen) ? bandH : 0.f;
			// Le bloc logo CHEVAUCHE les deux bandes : les bandes commencent a sa
			// droite, jamais au bord de la fenetre.
			const float32 logoW = (mHeaderLogo > 0.f && !fullScreen) ? mHeaderLogo : 0.f;
			// La bande basse n'est reservee que si quelqu'un l'affiche : la barre
			// d'etat du shell (visible) ou le hook de l'application.
			const float32 footerH =
				(fullScreen || (!mStatusBarVisible && !mStatusBarFn)) ? 0.f : mUI.S(22.f);
			// Largeur des bandes d'icones, PAR COTE : une app sans « vues » a
			// basculer les desactive (SetActivityBars) et le dock recupere la place.
			const float32 activityW = mUI.S(48.f);
			const float32 actWL = mActivityBarLeft ? activityW : 0.f;
			const float32 actWR = mActivityBarRight ? activityW : 0.f;
			// ── LES RAILS DE PASTILLES (§13) ────────────────────────────────
			// ⚠️ 28 PIXELS, SANS `S()` -- le document le dit, la capture doit le
			//    rendre. Et un rail SANS pastille ne prend AUCUNE place : une
			//    bande vide de 28 px serait du chrome, exactement ce qu on vient
			//    de retirer avec les barres d activite.
			const float32 railW = 28.f;
			const float32 railL = mRailCount[0] > 0 ? railW : 0.f;
			const float32 railR = mRailCount[1] > 0 ? railW : 0.f;
			const float32 railB = mRailCount[2] > 0 ? railW : 0.f;

			// Barre de titre custom UNE ligne : logo + menus | infos | min/max/close.
			DrawTitleBar(ec, {logoW, 0.f, W - logoW, titleH});
			// Barre d'outils Visual Studio (config/plateforme cible + Build/Run + emulateur).
			if (mToolbarFn && !fullScreen)
				DrawToolbar(ec, {logoW, titleH, W - logoW, toolbarH});
			// ⚠️ LE BLOC LOGO EST DESSINE APRES LES DEUX BANDES, et c est la seule
			//    facon de le faire CHEVAUCHER : il est plus haut que la premiere
			//    bande, donc il ne peut pas vivre dedans.
			if (logoW > 0.f)
				DrawHeaderLogo(ec, {0.f, 0.f, logoW, logoW});

			const float32 bodyTop = titleH + toolbarH;
			const float32 bodyH = H - bodyTop - footerH;

			// MODALE : quand Preferences est ouvert, le corps (panneaux/editeur)
			// ne doit pas reagir aux clics/molette/frappes destines au popup. On
			// masque l'input pour le corps puis on le restaure pour DrawPreferences.
			// IDEM quand la souris est au-dessus d'un menu DEROULANT ouvert (deja
			// dessine + gere dans la barre de titre ci-dessus) : sinon l'editeur /
			// les zones a hit-test « brut » derriere le menu recoivent les clics.
			// ATTENTION : ce masquage vise les menus de la BARRE DE TITRE, deja
			// dessines avant les panneaux. Mais `popupDepth` est partage avec les
			// popups que les PANNEAUX ouvrent eux-memes (BeginCombo, BeginMenu) —
			// et ceux-la sont dessines PENDANT DrawPanels, donc avec l'input
			// masque. Symptome : le combo s'ouvre, puis plus aucun clic ne passe
			// et il refuse de se refermer.
			//
			// Une application dont les panneaux utilisent les popups NKGui coupe
			// donc ce masquage (SetMaskBodyOnPopup(false)) : NKGui resout deja
			// l'occlusion de ses propres popups dans ItemHoverable. Elle doit en
			// contrepartie garder ses hit-tests « bruts » sous garde
			// `ctx.popupDepth == 0`.
			bool overPopup = false;
			for (int32 i = 0; i < mUI.popupDepth; ++i)
				if (nkgui::NkGuiRectContains(mUI.popupRects[i], mUI.input.mousePos)) {
					overPopup = true;
					break;
				}
			if (!mMaskBodyOnPopup)
				overPopup = false;
			// ② UNE MODALE QUI S'EST DECLAREE (NkGuiInput::ReserverSaisie) COMPTE COMME MODALE.
			//    C'est ce qui fait entrer ici les dialogues dessines par l'APPLICATION dans le
			//    crochet d'overlay -- le selecteur de fichier, en particulier : ils arrivent
			//    apres les panneaux, donc ni `appModal` ni `overPopup` ne les voyaient.
			const bool modal = mShowPrefs || mUI.appModal || overPopup || mCtxOpen || mUI.input.saisieReserveePrec;
			nkgui::NkGuiInput savedInput;
			if (modal) {
				savedInput = mUI.input;
				mPopupMasked = overPopup && !mShowPrefs && !mUI.appModal; // cf. dockHeaderFn
				mRealInput = savedInput;
				mUI.input.mousePos = {-100000.f, -100000.f};
				for (int32 i = 0; i < 3; ++i) {
					mUI.input.mouseClicked[i] = false;
					mUI.input.mouseDown[i] = false;
					mUI.input.mouseDoubleClicked[i] = false;
				}
				mUI.input.wheel = mUI.input.wheelH = 0.f;
				mUI.input.charCount = 0;
				mUI.input.wantCopy = mUI.input.wantCut = mUI.input.wantPaste = mUI.input.wantSelectAll = false;
				// ② ET LES TOUCHES (2026-09-05) : le bloc neutralisait la souris, la molette et
				//    les caracteres, JAMAIS l'etat des touches. `Suppr` atteignait donc la toile
				//    sous un dialogue ouvert, et supprimait la selection. On efface l'appui ET le
				//    front ; `keyPrev` n'est pas touche, et tout est restaure d'un bloc apres les
				//    panneaux (`mUI.input = savedInput`) : l'overlay voit l'entree reelle.
				for (int32 ki = 0; ki < nkgui::NkGuiInput::KeyCount; ++ki) {
					mUI.input.keyDown[ki] = false;
					mUI.input.keyInit[ki] = false;
				}
				mUI.input.ctrlDown = mUI.input.shiftDown = mUI.input.altDown = false;
			}

			if (fullScreen) {
				// Launcher : remplace barre d'activite + dock + panneaux.
				mStartScreenFn(ec, mStartScreenUser);
			} else {
				if (mActivityBarLeft)
					DrawActivityBar({0.f, bodyTop, actWL, bodyH});
				if (mActivityBarRight)
					DrawActivityBarRight({W - actWR, bodyTop, actWR, bodyH}); // IA (panneau droit)
				// ⚠️ LE DOCK NE RETRANCHE QUE LES RAILS, JAMAIS LE TIROIR. C est la
				//    ligne qui distingue l etat 2 de l etat 3 : si la largeur du
				//    tiroir apparaissait ici, deplier une pastille REDIMENSIONNERAIT
				//    le canvas, et l etat 2 serait devenu l etat 3 sans decision.
				const NkRect corps = {actWL + railL, bodyTop,
									  W - actWL - actWR - railL - railR, bodyH - railB};
				if (railL > 0.f)
					DrawRail(0, {actWL, bodyTop, railW, bodyH - railB}, true);
				if (railR > 0.f)
					DrawRail(1, {W - actWR - railW, bodyTop, railW, bodyH - railB}, true);
				if (railB > 0.f)
					DrawRail(2, {actWL, bodyTop + bodyH - railW, W - actWL - actWR, railW},
							 false);
				DockSpace(mUI, "##EditorDock", corps);
				// Seul le panneau CENTRAL masque la barre d'onglets de sa feuille quand il
				// est seul (il affiche ses propres onglets de fichiers) ; Terminal/Sortie/
				// sidebars gardent TOUJOURS leurs onglets, même seuls (façon VSCode).
				for (int32 i = 0; i < mNumPanels; ++i)
					if (mPanels[i])
						// Costume Banani (SetSideTabsVisible(false)) : les panneaux
						// LATÉRAUX aussi masquent leur barre d'onglets quand ils sont
						// seuls — ils dessinent alors leur propre en-tête de 34 px.
						DockWindowHideSingleTab(mUI, mPanels[i]->Title(),
												!mSideTabsVisible
													|| mPanels[i]->DefaultSide() == NkEditorDockSide::NK_CENTER);
				BootstrapDocking();
				DrawPanels(ec);
				// ⚠️ APRES LES PANNEAUX : le tiroir passe PAR-DESSUS le dock. Pose
				//    avant, il finirait derriere, et on croirait qu il ne s ouvre
				//    pas.
				DrawRailDrawers(ec, corps);
				if (mStatusBarFn) {
					// Barre d'etat « a sa maniere » (SetStatusBarFn, patron SetMenuBar) :
					// l'app dessine TOUTE la bande — fond, voyants, textes, zoom compris.
					// Region de layout posee sur le rect de la barre, comme DrawToolbar,
					// pour que l'app puisse y poser des widgets (Button + SameLine...).
					const NkRect sbar = {0.f, H - footerH, W, footerH};
					const float32 sbItemH = mUI.ItemHeight();
					mUI.layout.region = sbar;
					mUI.layout.cursor = {sbar.x + mUI.S(8.f), sbar.y + (sbar.h - sbItemH) * 0.5f};
					mUI.layout.lineStartX = mUI.layout.cursor.x;
					mUI.layout.curLineH = 0.f;
					mUI.layout.maxX = mUI.layout.cursor.x;
					mStatusBarFn(ec, mStatusBarUser);
				} else {
					DrawStatusBar(footerH);
				}
			}
			HandleEdgeResize(W, H); // bords de redimensionnement (fenetre sans bordure)

			if (modal)
				mUI.input = savedInput; // restaure pour le popup
			mPopupMasked = false;
			DrawContextMenu(); // menu contextuel shell-level (au-dessus des panneaux)
			// (DrawFilePicker retiré : add-folder réutilise LE picker de l'app, Dialogs.h.
			//  Le picker fichier/dossier UNIFIÉ sera extrait dans NKEditorKit — phase 2.)
			DrawCommandPalette(ec);
			DrawPreferences(ec); // fenetre Preferences (menu dedie)
			if (mOverlayFn)
				mOverlayFn(ec, mOverlayUser); // dialogues modaux de l'app (creation/proprietes)

			// Bordure de NOTRE fenetre (l'OS n'en dessine plus) — sauf si maximisee.
			if (!mWindow.IsMaximized())
				mUI.dlOverlay.AddRect({0.f, 0.f, W, H}, {48, 54, 61, 255}, 1.f); // bord #30363d

			mUI.EndFrame();

			mWindow.SetCursor(MapCursor(mUI.wantCursor));

			mRenderer->BeginFrame();
			mRenderer->SubmitDrawList(mUI.dl, sz.x, sz.y);
			mRenderer->SubmitDrawList(mUI.dlOverlay, sz.x, sz.y);
			mRenderer->EndFrame();
		}

		// ── Activity bar (bande verticale d'icones a gauche, facon VSCode) ────────
		// ═══════════════════════════════════════════════════════════════════════
		//  LES RAILS DE PASTILLES (document 3 §13) — etats 1 et 2
		// ═══════════════════════════════════════════════════════════════════════
		void NkEditorShell::SetRail(NkEditorDockSide side, const NkEditorRailItem *items,
									int32 count) noexcept {
			int32 slot = -1;
			if (side == NkEditorDockSide::NK_LEFT)
				slot = 0;
			else if (side == NkEditorDockSide::NK_RIGHT)
				slot = 1;
			else if (side == NkEditorDockSide::NK_BOTTOM)
				slot = 2;
			if (slot < 0) {
				// ⚠️ NK_TOP et NK_CENTER N ONT PAS DE RAIL, et le dire vaut mieux
				//    que d ignorer : une application qui pose un rail en haut
				//    verrait simplement rien apparaitre, et chercherait le defaut
				//    dans son dessin.
				logger.Error("[NkEditorShell] SetRail : seuls NK_LEFT, NK_RIGHT et "
							 "NK_BOTTOM portent un rail. Rien n'a ete pose.");
				return;
			}
			if (count > kRailMax) {
				// ⚠️ LE PLAFOND CRIE. Un rail qui garderait les huit premieres et
				//    laisserait tomber le reste donnerait une pastille absente sans
				//    aucune trace -- et on chercherait pourquoi le panneau
				//    « n existe pas ».
				logger.Error("[NkEditorShell] SetRail : {0} pastilles demandées, plafond "
							 "kRailMax={1}. Les suivantes sont REFUSÉES, pas ignorées.",
							 count, (int32)kRailMax);
				count = kRailMax;
			}
			mRailCount[slot] = count < 0 ? 0 : count;
			for (int32 i = 0; i < mRailCount[slot]; ++i)
				mRailItems[slot][i] = items[i];
			mRailOuvert[slot] = -1;
		}

		NkEditorPanel *NkEditorShell::TrouverPanneau(const char *titre) noexcept {
			if (!titre || !*titre)
				return nullptr;
			for (int32 i = 0; i < mNumPanels; ++i) {
				if (!mPanels[i])
					continue;
				const char *t = mPanels[i]->Title();
				int32 k = 0;
				while (t[k] && titre[k] && t[k] == titre[k])
					++k;
				if (t[k] == '\0' && titre[k] == '\0')
					return mPanels[i];
			}
			return nullptr;
		}

		// ETAT 1 : la pastille repliee, avec son infobulle.
		void NkEditorShell::DrawRail(int32 slot, const NkRect &bar, bool vertical) noexcept {
			auto &dl = mUI.dl;
			dl.AddRectFilled(bar, mUI.theme.header);
			// ⚠️ 28 PIXELS, ET ILS NE PASSENT PAS PAR `S()`. Le document dit 28 px
			//    et la mesure sur la capture doit rendre 28 -- meme regle que les
			//    deux bandes de l en-tete. Un facteur DPI donnerait 30 a 107 %.
			const float32 cell = 28.f;
			const NkVec2 m = mUI.input.mousePos;
			const bool ptrOk = (mUI.hoveredWindowId == NKGUI_ID_NONE);

			// Costume Banani (2026-08-31) : dès qu'une pastille du rail porte son
			// propre dessin (`icone`), le rail prend la géométrie de la maquette —
			// marge 8, écart 4, hauteur de pilule 20 au rail bas — et le CONTENU
			// est dessiné par l'application. Sans `icone`, rien ne bouge.
			bool costume = false;
			for (int32 i = 0; i < mRailCount[slot]; ++i)
				if (mRailItems[slot][i].icone)
					costume = true;
			float32 curseur = costume ? 8.f : 4.f;

			for (int32 i = 0; i < mRailCount[slot]; ++i) {
				const NkEditorRailItem &it = mRailItems[slot][i];
				const float32 lg = (it.largeur > 0.f) ? it.largeur : cell;
				NkRect r;
				if (vertical)
					r = {bar.x, bar.y + curseur, cell, cell};
				else if (costume)
					r = {bar.x + curseur, bar.y + (bar.h - 20.f) * 0.5f, lg, 20.f};
				else
					r = {bar.x + curseur, bar.y, lg, cell};
				curseur += (vertical ? cell : lg) + (costume ? 4.f : 0.f);
				const bool hov = ptrOk && m.x >= r.x && m.x < r.x + r.w && m.y >= r.y
								 && m.y < r.y + r.h;
				const bool ouvert = (mRailOuvert[slot] == i);

				if (it.icone) {
					// Fond d'état seulement (accent à 13 % quand déplié — Banani
					// SideRail —, survol discret sinon) ; le contenu vient de l'app.
					if (ouvert) {
						NkColor voile = mUI.theme.accent;
						voile.a = 34; // ≈ 13 % (la pilule #2f81f722 de la maquette)
						dl.AddRectFilled(r, voile, 4.f);
					} else if (hov)
						dl.AddRectFilled(r, mUI.theme.buttonHover, 4.f);
					it.icone(mUI, r, ouvert, hov, it.iconeUser);
				} else {
					if (ouvert)
						dl.AddRectFilled(r, mUI.theme.accent, 4.f);
					else if (hov)
						dl.AddRectFilled(r, mUI.theme.buttonHover, 4.f);

					if (mUI.font && mUI.font->Valid() && it.glyphe && *it.glyphe) {
						const float32 w = mUI.font->MeasureWidth(it.glyphe);
						const float32 by = r.y + (r.h - mUI.font->LineHeight()) * 0.5f
										   + mUI.font->Ascent();
						dl.AddText(mUI.font->Face(), mUI.font->TexId(),
								   {r.x + (r.w - w) * 0.5f, by}, it.glyphe,
								   ouvert ? mUI.theme.onAccent
										  : (hov ? mUI.theme.text : mUI.theme.textDisabled));
					}
				}

				// L infobulle de l etat 1. ⚠️ REPRISE, PAS REECRITE : `NkTooltip`
				//    sert deja aux voyants du pied de fenetre.
				NkTooltip(mUI, hov, it.tooltip && *it.tooltip ? it.tooltip : it.panel);

				if (hov && mUI.input.mouseClicked[0]) {
					// §13.3 : deplier la seconde referme la premiere. La regle est
					// structurelle -- `mRailOuvert` est UN entier.
					mRailOuvert[slot] = ouvert ? -1 : i;
					// ⚠️ LE CLIC EST CONSOMME. Sans ca, le meme clic servait a
					//    ouvrir la pastille PUIS a la refermer par la regle du
					//    « clic ailleurs » quelques lignes plus bas : la pastille
					//    clignotait sans jamais rester ouverte.
					mUI.input.mouseClicked[0] = false;
				}
			}

			// LE TEXTE DU RAIL BAS (2026-08-30, fusion des bandeaux §4/§13) :
			// l'aide contextuelle vit dans l'espace restant, a droite des
			// pastilles — un bandeau, deux contenus, zero second etage.
			// La pastille d'état à DROITE (« ● Prêt », Banani BottomRail) : point
			// de 6 px + texte, alignés au bord droit. Le texte d'aide s'arrête
			// avant elle.
			float32 statusLx = bar.x + bar.w;
			if (!vertical && mRailStatusText[0] && mUI.font && mUI.font->Valid()) {
				const float32 tw = mUI.font->MeasureWidth(mRailStatusText);
				const float32 tx = bar.x + bar.w - 8.f - tw;
				const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f
								   + mUI.font->Ascent();
				dl.AddText(mUI.font->Face(), mUI.font->TexId(), {tx, by}, mRailStatusText,
						   mUI.theme.textDisabled);
				const float32 dy = bar.y + bar.h * 0.5f;
				dl.AddCircleFilled({tx - 4.f - 3.f, dy}, 3.f, mRailStatusColor);
				statusLx = tx - 4.f - 6.f - 8.f;
			}
			if (!vertical && mRailFooterText[0] && mUI.font && mUI.font->Valid()) {
				const float32 tx = bar.x + curseur + 12.f;
				const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f
								   + mUI.font->Ascent();
				dl.PushClipRect({tx, bar.y, statusLx - tx - 8.f, bar.h}, true);
				dl.AddText(mUI.font->Face(), mUI.font->TexId(), {tx, by}, mRailFooterText,
						   mUI.theme.textDisabled);
				dl.PopClipRect();
			}
		}

		// ETAT 2 : le tiroir, EN OVERLAY par-dessus le canvas.
		void NkEditorShell::DrawRailDrawers(NkEditorFrameContext &ec, const NkRect &corps) noexcept {
			const float32 cell = 28.f;
			const float32 taille = 320.f; // §13.2 : « largeur/hauteur par defaut ~320px »
			bool clicDansUnTiroir = false;

			for (int32 slot = 0; slot < 3; ++slot) {
				const int32 i = mRailOuvert[slot];
				if (i < 0 || i >= mRailCount[slot])
					continue;
				NkEditorPanel *p = TrouverPanneau(mRailItems[slot][i].panel);

				NkRect d;
				if (slot == 0)
					d = {corps.x + cell, corps.y, taille, corps.h};
				else if (slot == 1)
					d = {corps.x + corps.w - cell - taille, corps.y, taille, corps.h};
				else
					d = {corps.x, corps.y + corps.h - cell - 240.f, corps.w, 240.f};

				// ⚠️ TOUT CECI VA DANS LA COUCHE OVERLAY. Le tiroir doit passer
				//    PAR-DESSUS les panneaux ancres ; dessine dans la couche
				//    normale, il finirait derriere le dock et on croirait qu il ne
				//    s ouvre pas.
				PushOverlay(mUI);
				// Le voile : il dit « ce qui est dessous attend ». Sans lui, le
				// tiroir se lit comme un panneau de plus, pas comme un tiroir.
				mUI.dlOverlay.AddRectFilled(corps, mUI.theme.scrim);
				mUI.dlOverlay.AddRectFilled(d, mUI.theme.panel, 6.f);
				mUI.dlOverlay.AddRect(d, mUI.theme.border, 1.f, 6.f);

				const float32 titreH = mUI.ItemHeight() + 6.f;
				if (mUI.font && mUI.font->Valid()) {
					const char *t = p ? p->Title() : mRailItems[slot][i].panel;
					mUI.dlOverlay.AddText(mUI.font->Face(), mUI.font->TexId(),
										  {d.x + 10.f, d.y + (titreH - mUI.font->LineHeight()) * 0.5f
														   + mUI.font->Ascent()},
										  t, mUI.theme.text);
				}
				mUI.dlOverlay.AddLine({d.x, d.y + titreH}, {d.x + d.w, d.y + titreH},
									  mUI.theme.border, 1.f);

				const NkRect dedans = {d.x, d.y + titreH, d.w, d.h - titreH};
				if (p) {
					// ⚠️ LE TIROIR DESSINE UN PANNEAU EXISTANT, il n en invente pas
					//    un second. C est ce qui rendra l etat 3 (l ancrer) presque
					//    gratuit : le meme objet, ancre au lieu d etre pose ici.
					if (BeginChild(mUI, "##tiroir", dedans, false, true)) {
						p->OnUI(ec);
						EndChild(mUI);
					}
				} else if (mUI.font && mUI.font->Valid()) {
					// ⚠️ UNE PASTILLE QUI NE TROUVE PAS SON PANNEAU LE DIT. Un
					//    tiroir vide ressemble a un panneau casse ; cette ligne
					//    nomme la cle qui n a rien trouve, et la cle est la seule
					//    chose qui puisse etre fausse.
					mUI.dlOverlay.AddText(mUI.font->Face(), mUI.font->TexId(),
										  {dedans.x + 10.f, dedans.y + 24.f},
										  "Aucun panneau enregistre sous ce titre.",
										  mUI.theme.danger);
				}
				PopOverlay(mUI);

				const NkVec2 m = mUI.input.mousePos;
				if (m.x >= d.x && m.x < d.x + d.w && m.y >= d.y && m.y < d.y + d.h)
					clicDansUnTiroir = true;
			}

			// §13.2 : « un clic ailleurs sur le canvas la referme ». ⚠️ Le clic sur
			// la PASTILLE a deja ete consomme plus haut : sans cette consommation,
			// ouvrir et refermer se produisaient dans la meme image.
			if (mUI.input.mouseClicked[0] && !clicDansUnTiroir) {
				for (int32 slot = 0; slot < 3; ++slot)
					mRailOuvert[slot] = -1;
			}
		}

		void NkEditorShell::DrawActivityBar(const NkRect &bar) noexcept {
			auto &dl = mUI.dl;
			const NkColor barBg = mUI.theme.header; // theme-aware (suit Dark/Light)
			const NkColor on = mUI.theme.text;
			const NkColor off = mUI.theme.textDisabled;
			const NkColor hov = mUI.theme.text;
			const NkColor accent = mUI.theme.accent;
			dl.AddRectFilled(bar, barBg);
			if (!mUI.font) {
			}
			const float32 cell = bar.w; // cellule carree = largeur barre
			const NkVec2 m = mUI.input.mousePos;

			// Icones vectorielles (dessinees relativement au centre (cx,cy)).
			// Souris au-dessus d'une fenêtre flottante -> la barre ne réagit pas (occlusion).
			const bool ptrOk = (mUI.hoveredWindowId == NKGUI_ID_NONE);
			auto drawIcon = [&](int32 idx, float32 cy, bool bottom) {
				const NkRect r = {bar.x, cy - cell * 0.5f, cell, cell};
				const bool hovered = ptrOk && m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
				const bool active = (mActivityIndex == idx) && !bottom;
				const NkColor c = active ? on : hovered ? hov : off;
				if (active)
					dl.AddRectFilled({bar.x, r.y, mUI.S(2.f), cell}, accent); // barre d'accent
				const float32 cx = bar.x + cell * 0.5f, ic = cy;
				const float32 s = mUI.S(8.f);
				// Texture fournie par l'app ? -> icône codicon TEINTÉE, sinon dessin.
				{
					const uint32 tex = bottom ? mActTexGear : ((idx >= 0 && idx < 8) ? mActTexL[idx] : 0u);
					if (tex) {
						const float32 hs = s * 1.15f;
						dl.AddImage(tex, {cx - hs, ic - hs, hs * 2.f, hs * 2.f}, {0.f, 0.f}, {1.f, 1.f}, c);
						if (hovered && mUI.input.mouseClicked[0]) {
							if (!bottom)
								mActivityIndex = idx;
							if (mActivityFn)
								mActivityFn(mActivityUser, idx);
						}
						return;
					}
				}
				switch (idx) {
					case 0: { // Explorateur : un document + lignes de texte
						dl.AddRect({cx - s * 0.8f, ic - s, s * 1.6f, s * 2.f}, c, 1.5f);
						for (int k = 0; k < 3; ++k) {
							const float32 ly = ic - s * 0.4f + k * s * 0.6f;
							dl.AddLine({cx - s * 0.5f, ly}, {cx + s * 0.45f, ly}, c, 1.f);
						}
					} break;
					case 1: { // Recherche : anneau + manche
						dl.AddCircleFilled({cx - s * 0.25f, ic - s * 0.25f}, s * 0.7f, c);
						dl.AddCircleFilled({cx - s * 0.25f, ic - s * 0.25f}, s * 0.45f, barBg);
						dl.AddLine({cx + s * 0.3f, ic + s * 0.3f}, {cx + s, ic + s}, c, 2.f);
					} break;
					case 2: { // Controle de source : branche (3 noeuds + liens)
						const NkVec2 a = {cx - s * 0.6f, ic - s * 0.8f};
						const NkVec2 b = {cx - s * 0.6f, ic + s * 0.8f};
						const NkVec2 d = {cx + s * 0.7f, ic};
						dl.AddLine(a, b, c, 1.5f);
						dl.AddLine({cx - s * 0.6f, ic}, d, c, 1.5f);
						dl.AddCircleFilled(a, s * 0.35f, c);
						dl.AddCircleFilled(b, s * 0.35f, c);
						dl.AddCircleFilled(d, s * 0.35f, c);
					} break;
					case 3: { // Executer/Deboguer : triangle play
						dl.AddTriangleFilled({cx - s * 0.6f, ic - s}, {cx - s * 0.6f, ic + s}, {cx + s, ic}, c);
					} break;
					case 4: { // Live Collab : deux personnes (tetes + epaules)
						dl.AddCircleFilled({cx - s * 0.45f, ic - s * 0.45f}, s * 0.38f, c);
						dl.AddCircleFilled({cx + s * 0.5f, ic - s * 0.25f}, s * 0.32f, c);
						dl.AddRectFilled({cx - s * 0.95f, ic + s * 0.05f, s * 1.f, s * 0.75f}, c, s * 0.4f);
						dl.AddRectFilled({cx + s * 0.08f, ic + s * 0.2f, s * 0.85f, s * 0.6f}, c, s * 0.35f);
					} break;
					case 5: { // Extensions : 4 carres (un detache)
						const float32 q = s * 0.6f;
						dl.AddRectFilled({cx - s, ic - s, q, q}, c);
						dl.AddRectFilled({cx - s + q + 2.f, ic - s, q, q}, c);
						dl.AddRectFilled({cx - s, ic - s + q + 2.f, q, q}, c);
						dl.AddRect({cx + 2.f, ic + 2.f, q, q}, c, 1.5f);
					} break;
					case 6: { // Profiler : histogramme (3 barres)
						dl.AddRectFilled({cx - s, ic - s * 0.1f, s * 0.5f, s * 1.1f}, c);
						dl.AddRectFilled({cx - s * 0.25f, ic - s, s * 0.5f, s * 2.f}, c);
						dl.AddRectFilled({cx + s * 0.5f, ic - s * 0.5f, s * 0.5f, s * 1.5f}, c);
					} break;
					case 100: { // Claude Code : asterisque (6 branches)
						dl.AddLine({cx, ic - s}, {cx, ic + s}, c, 2.f);
						dl.AddLine({cx - s * 0.87f, ic - s * 0.5f}, {cx + s * 0.87f, ic + s * 0.5f}, c, 2.f);
						dl.AddLine({cx - s * 0.87f, ic + s * 0.5f}, {cx + s * 0.87f, ic - s * 0.5f}, c, 2.f);
					} break;
					case 101: { // Codex : chevrons < >
						dl.AddLine({cx - s * 0.2f, ic - s}, {cx - s, ic}, c, 2.f);
						dl.AddLine({cx - s, ic}, {cx - s * 0.2f, ic + s}, c, 2.f);
						dl.AddLine({cx + s * 0.2f, ic - s}, {cx + s, ic}, c, 2.f);
						dl.AddLine({cx + s, ic}, {cx + s * 0.2f, ic + s}, c, 2.f);
					} break;
					case 102: { // IA Maison : maison (toit + corps)
						dl.AddTriangleFilled({cx, ic - s}, {cx - s, ic - s * 0.1f}, {cx + s, ic - s * 0.1f}, c);
						dl.AddRectFilled({cx - s * 0.65f, ic - s * 0.05f, s * 1.3f, s * 1.f}, c);
						dl.AddRectFilled({cx - s * 0.18f, ic + s * 0.35f, s * 0.36f, s * 0.6f}, barBg);
					} break;
					case 999: { // Reglages : roue dentee (anneau + centre)
						dl.AddCircleFilled({cx, ic}, s * 0.9f, c);
						dl.AddCircleFilled({cx, ic}, s * 0.5f, barBg);
						dl.AddCircleFilled({cx, ic}, s * 0.2f, c);
					} break;
					default:
						break;
				}
				// Clic : l'APP decide (sidebar exclusive facon VSCode) via SetActivityHandler ;
				// sans handler : comportement historique (icone 0 = bascule le 1er panneau gauche).
				if (hovered && mUI.input.mouseClicked[0]) {
					if (!bottom)
						mActivityIndex = idx;
					if (mActivityFn)
						mActivityFn(mActivityUser, idx);
					else if (idx == 0) {
						for (int32 p = 0; p < mNumPanels; ++p)
							if (mPanels[p]->DefaultSide() == NkEditorDockSide::NK_LEFT) {
								mPanels[p]->SetOpen(!mPanels[p]->IsOpen());
								break;
							}
					}
				}
			};

			const float32 top = bar.y + cell * 0.5f;
			for (int32 i = 0; i < 7; ++i)			// vues GAUCHE : explorateur, recherche, git, debug, collab,
				drawIcon(i, top + i * cell, false); // extensions, profiler
			drawIcon(999, bar.y + bar.h - cell * 0.5f, true); // Reglages en bas
			// Consomme le clic dans l'activity bar -> il ne FUIT PAS vers les panneaux dessous
			// (sinon un clic d'icone traverse jusqu'a la barre d'onglets de l'editeur = onglet actif change).
			if ((mUI.input.mouseClicked[0] || mUI.input.mouseClicked[1]) && m.x >= bar.x && m.x < bar.x + bar.w &&
				m.y >= bar.y && m.y < bar.y + bar.h) {
				mUI.input.mouseClicked[0] = false;
				mUI.input.mouseClicked[1] = false;
			}
		}

		// ── Activity bar DROITE : les systèmes d'IA (panneau latéral droit exclusif). ──
		void NkEditorShell::DrawActivityBarRight(const NkRect &bar) noexcept {
			auto &dl = mUI.dl;
			const NkColor barBg = mUI.theme.header;
			const NkColor off = mUI.theme.textDisabled;
			const NkColor onC = mUI.theme.text;
			dl.AddRectFilled(bar, barBg);
			const float32 cell = bar.w;
			const NkVec2 m = mUI.input.mousePos;
			// Souris au-dessus d'une fenêtre flottante -> la barre ne réagit pas (occlusion).
			const bool ptrOk = (mUI.hoveredWindowId == NKGUI_ID_NONE);
			auto icon = [&](int32 idx, float32 cy) {
				const NkRect r = {bar.x, cy - cell * 0.5f, cell, cell};
				const bool hovered = ptrOk && m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
				const bool active = (mActivityIndexRight == idx);
				const NkColor c = (active || hovered) ? onC : off;
				if (active) // barre d'accent au bord DROIT (miroir de la barre gauche)
					dl.AddRectFilled({bar.x + bar.w - mUI.S(2.f), r.y, mUI.S(2.f), cell}, mUI.theme.accent);
				// Texture fournie par l'app ? -> icône codicon TEINTÉE, sinon dessin.
				if (idx >= 100 && idx <= 103 && mActTexR[idx - 100]) {
					const float32 hs = mUI.S(9.f);
					dl.AddImage(mActTexR[idx - 100], {bar.x + cell * 0.5f - hs, cy - hs, hs * 2.f, hs * 2.f},
								{0.f, 0.f}, {1.f, 1.f}, c);
					if (hovered && mUI.input.mouseClicked[0] && mActivityFn)
						mActivityFn(mActivityUser, idx);
					return;
				}
				const float32 cx = bar.x + cell * 0.5f, ic = cy, s = mUI.S(8.f);
				switch (idx) {
					case 100: // Claude Code : asterisque
						dl.AddLine({cx, ic - s}, {cx, ic + s}, c, 2.f);
						dl.AddLine({cx - s * 0.87f, ic - s * 0.5f}, {cx + s * 0.87f, ic + s * 0.5f}, c, 2.f);
						dl.AddLine({cx - s * 0.87f, ic + s * 0.5f}, {cx + s * 0.87f, ic - s * 0.5f}, c, 2.f);
						break;
					case 101: // Codex : chevrons < >
						dl.AddLine({cx - s * 0.2f, ic - s}, {cx - s, ic}, c, 2.f);
						dl.AddLine({cx - s, ic}, {cx - s * 0.2f, ic + s}, c, 2.f);
						dl.AddLine({cx + s * 0.2f, ic - s}, {cx + s, ic}, c, 2.f);
						dl.AddLine({cx + s, ic}, {cx + s * 0.2f, ic + s}, c, 2.f);
						break;
					case 102: // IA Maison : maison
						dl.AddTriangleFilled({cx, ic - s}, {cx - s, ic - s * 0.1f}, {cx + s, ic - s * 0.1f}, c);
						dl.AddRectFilled({cx - s * 0.65f, ic - s * 0.05f, s * 1.3f, s * 1.f}, c);
						dl.AddRectFilled({cx - s * 0.18f, ic + s * 0.35f, s * 0.36f, s * 0.6f}, barBg);
						break;
					default:
						break;
				}
				if (hovered && mUI.input.mouseClicked[0] && mActivityFn)
					mActivityFn(mActivityUser, idx);
			};
			const float32 top = bar.y + cell * 0.5f;
			icon(100, top);			   // Claude Code
			icon(101, top + cell);	   // Codex
			icon(102, top + cell * 2); // Assistant IA
			icon(103, top + cell * 3); // NkAI (IA maison)
		}

		// ── Barre de titre custom (UNE ligne : logo + menus | infos | controles) ──
		// Layout facon VSCode : [logo][Fichier Affichage ...]   <infos centre>   [─ ☐ ✕]
		//
		// ═══════════════════════════════════════════════════════════════════════
		//  LE BLOC LOGO : LA MARQUE RIHEN, LE « O »
		// ═══════════════════════════════════════════════════════════════════════
		//  Un disque COUPE AU-DESSUS DE SON CENTRE : petite face haute, grande
		//  face basse, et un VIDE entre les deux. Ce n'est pas un demi-disque —
		//  une coupe au centre donnerait deux moities egales, c'est-a-dire une
		//  autre marque.
		//
		// ⚠️ CE QU IL Y AVAIT AVANT, ET POURQUOI CA NE POUVAIT PAS RESTER : une
		//    grille de 2x2 carres, posee pour que le bloc existe des la premiere
		//    image. Un bouche-trou. Or ce bloc est l ANCRE VISUELLE de la fenetre,
		//    le premier element que l oeil rencontre — un bouche-trou y est plus
		//    visible que partout ailleurs, et il se lit comme la marque.
		//
		// ⚠️ ET IL EST DANS LE KIT, PAS DANS L APPLICATION. Toutes les
		//    applications Rihen (NKCode, NK3DModeler, Nogee, NkAnimaEditor,
		//    NkUIDesign, PV3DE) portent la MEME marque. La definir dans chaque
		//    application, ce serait six dessins qui divergent des la premiere
		//    retouche — exactement le raisonnement qui met les themes ici.
		//
		// ⚠️ TRACEE, PAS CHARGEE. Une texture exigerait un decodeur, un chemin de
		//    ressource et un cas « fichier absent » — alors que ce bloc doit
		//    exister DES LA PREMIERE IMAGE, meme sans aucune ressource. Deux
		//    polygones convexes n exigent rien. Une application qui veut son
		//    propre logo pose `SetTitleLogo`, teste en premier ci-dessous.
		//
		//  Couleurs : charte Rihen — petrole #0A555F (face haute), orange #F79A28
		//  (face basse). Ce sont des couleurs de MARQUE, pas des roles de theme :
		//  une marque qui change avec le theme n est plus une marque.
		//
		// ⚠️ REPERE ECRAN : **y croit vers le BAS**. Un point d angle `a` est donc
		//    (cx + R cos a, cy + R sin a) avec a = -pi/2 EN HAUT et a = +pi/2 EN
		//    BAS. Ma premiere version a raisonne en repere mathematique et a
		//    produit deux arcs qui se recouvraient. C est ecrit ici parce que
		//    l erreur ne se voit pas a la relecture du code, seulement a l ecran.
		// ⚠️ CE N ETAIT PAS UN « POINT DE SYNCHRONISATION », C ETAIT UNE
		//    CONVERSION -- et elle vivait ICI, dans un `namespace {}` anonyme,
		//    donc **inaccessible a l editeur de liens** pour toute application
		//    sans coquille. NK3DModeler n utilise deliberement pas
		//    `NkEditorShell` (`NkModelerUI.h:5`) : sa seule sortie etait d en
		//    ecrire une seconde. C est le mecanisme des deux registres de roles
		//    homonymes et des deux objets theme payes cette semaine -- **la
		//    deuxieme copie n est presque jamais un caprice, c est la seule porte
		//    restee ouverte.**
		//
		//    La conversion vit desormais dans `NKEditorKit/NkThemeToGui.h`, et
		//    `NkThemeUnpack` / `NkThemeMix` avec elle. Cette methode n est plus
		//    qu un APPELANT parmi d autres. **Deux appelants d une conversion ne
		//    font pas deux autorites.**
		void NkEditorShell::ApplyTheme(const NkTheme &t) noexcept {
			NkThemeVersGui(mUI, t);
		}

		void NkEditorShell::DrawHeaderLogo(NkEditorFrameContext &, const NkRect &r) noexcept {
			auto &dl = mUI.dl;
			dl.AddRectFilled(r, mUI.theme.header);
			// Costume exact (2026-08-31) : l'application dessine son propre bloc
			// logo (patron SetMenuBar). Le « O » Rihen reste le défaut du kit.
			if (mHeaderLogoFn) {
				mHeaderLogoFn(mUI, r, mHeaderLogoUser);
				return;
			}
			if (mTitleLogoTex) {
				const float32 g = r.w * 0.5f;
				dl.AddImage(mTitleLogoTex, {r.x + (r.w - g) * 0.5f, r.y + (r.h - g) * 0.5f, g, g},
							{0.f, 0.f}, {1.f, 1.f}, {255, 255, 255, 255});
				return;
			}

			const float32 kPi = 3.14159265f;
			const float32 rad = (r.w < r.h ? r.w : r.h) * 0.34f;
			const float32 cx = r.x + r.w * 0.5f;
			const float32 cy = r.y + r.h * 0.5f;
			const float32 coupe = cy - rad * 0.30f;	 // AU-DESSUS du centre
			const float32 vide = rad * 0.05f;		 // le VIDE entre les deux faces

			const NkColor kPetrole = {10, 85, 95, 255};	 // #0A555F
			const NkColor kOrange = {247, 154, 40, 255}; // #F79A28

			// ⚠️ 32 POINTS D ARC, PAS 8. A 56 px de bloc, un arc a 8 segments se
			//    lit comme un polygone : la marque devient anguleuse, et c est le
			//    genre de detail qu on ne voit qu une fois imprime.
			static const int32 kN = 32;
			NkVec2 poly[kN];

			// Un segment de disque coupe a l ordonnee `y` : la demi-corde vaut
			// R*sqrt(1 - (dy/R)^2), et les deux bouts de l arc sont aux angles
			// atan2(dy, +demiCorde) et atan2(dy, -demiCorde).
			// La FACE HAUTE va du bout GAUCHE au bout DROIT **par le haut** ;
			// la FACE BASSE va du bout DROIT au bout GAUCHE **par le bas**.
			// Les deux arcs suffisent : la corde ferme le polygone toute seule.
			const float32 dyH = (coupe - vide) - cy;
			const float32 kH = 1.f - (dyH / rad) * (dyH / rad);
			if (kH > 0.f) {
				const float32 demi = rad * nkentseu::math::NkSqrt(kH);
				const float32 aD = nkentseu::math::NkAtan2(dyH, demi);	// bout droit
				const float32 aG = -kPi - aD;							// bout gauche
				for (int32 i = 0; i < kN; ++i) {
					const float32 a = aG + (aD - aG) * ((float32)i / (float32)(kN - 1));
					poly[i] = {cx + rad * nkentseu::math::NkCos(a),
							   cy + rad * nkentseu::math::NkSin(a)};
				}
				dl.AddConvexPolyFilled(poly, kN, kPetrole);
			}

			const float32 dyB = (coupe + vide) - cy;
			const float32 kB = 1.f - (dyB / rad) * (dyB / rad);
			if (kB > 0.f) {
				const float32 demi = rad * nkentseu::math::NkSqrt(kB);
				const float32 aD = nkentseu::math::NkAtan2(dyB, demi);
				const float32 aG = -kPi - aD + 2.f * kPi; // le meme, un tour plus loin
				for (int32 i = 0; i < kN; ++i) {
					const float32 a = aD + (aG - aD) * ((float32)i / (float32)(kN - 1));
					poly[i] = {cx + rad * nkentseu::math::NkCos(a),
							   cy + rad * nkentseu::math::NkSin(a)};
				}
				dl.AddConvexPolyFilled(poly, kN, kOrange);
			}
		}

		void NkEditorShell::DrawTitleBar(NkEditorFrameContext &ec, const NkRect &bar) noexcept {
			auto &dl = mUI.dl;
			// Costume exact (2026-08-31) : police dédiée de la barre de titre —
			// menus et nom de fichier d'une maquette à 11 px cessent d'hériter de
			// la police d'interface. Échangée pour TOUTE la barre (menus déroulants
			// compris, dessinés pendant BuildMenuBar), restaurée à la sortie.
			nkgui::NkGuiFont *fontInterface = mUI.font;
			if (mTitleBarFont && mTitleBarFont->Valid())
				mUI.font = mTitleBarFont;
			const NkColor bg = mUI.theme.header; // barre de titre (suit Dark/Light)
			const NkColor fg = mUI.theme.text;
			const NkColor accent = mUI.theme.accent;
			const bool lightBar = (int32(bg.r) + bg.g + bg.b) > 384;
			dl.AddRectFilled(bar, bg);
			const NkVec2 m = mUI.input.mousePos;
			const float32 pad = mUI.S(8.f);
			const float32 cy = bar.y + bar.h * 0.5f;

			// Logo NKCode a l'extreme gauche. Wordmark complet (aspect>0) => icone+nom
			// deja dans l'image, on NE re-ecrit PAS "nkcode". Sinon icone carree (+ texte).
			float32 cursorX = bar.x + pad;
			bool wordmark = false;
			// ⚠️ QUAND LE BLOC LOGO CARRE EXISTE, LA BARRE N EN REDESSINE PAS UN.
			//    Mesure sur capture : un carre bleu de 12 px s intercalait entre le
			//    « O » Rihen et « Fichier » — le repli « pas de texture chargee » de
			//    cette barre, qui ignorait que le bloc a cheval sur les deux bandes
			//    porte deja la marque. **Deux logos a 40 px l un de l autre**, et le
			//    second etait un carre de remplissage.
			// ⚠️ ET LES MENUS PARTENT ALORS DU BORD DU BLOC, sans marge : le
			//    document 3 §5 l exige — « colle au bord droit du bloc logo, pas
			//    d espace mort entre le logo et le menu, ils forment un seul groupe
			//    visuel ».
			if (mHeaderLogo > 0.f) {
				cursorX = bar.x;
			} else if (mTitleLogoTex) {
				const float32 lg = bar.h * 0.62f;
				if (mTitleLogoAspect > 0.f) { // logo complet (ratio preserve)
					const float32 lw = lg * mTitleLogoAspect;
					dl.AddImage(mTitleLogoTex, {cursorX, cy - lg * 0.5f, lw, lg}, {0.f, 0.f}, {1.f, 1.f},
								{255, 255, 255, 255});
					cursorX += lw + mUI.S(10.f);
					wordmark = true;
				} else { // icone carree
					dl.AddImage(mTitleLogoTex, {cursorX, cy - lg * 0.5f, lg, lg}, {0.f, 0.f}, {1.f, 1.f},
								{255, 255, 255, 255});
					cursorX += lg + mUI.S(8.f);
				}
			} else {
				const float32 lg = mUI.S(12.f);
				dl.AddRectFilled({cursorX, cy - lg * 0.5f, lg, lg}, accent);
				cursorX += lg + mUI.S(8.f);
			}
			// Sur le launcher : nom "nkcode" (seulement si pas deja dans le wordmark) + AUCUN menu.
			float32 menuX = cursorX;
			if (mUI.appFullScreen && !wordmark && mUI.font && mUI.font->Face()) {
				const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
				dl.AddText(mUI.font->Face(), mUI.font->TexId(), {cursorX, by}, "nkcode", fg);
				menuX = cursorX + mUI.font->MeasureWidth("nkcode") + mUI.S(12.f);
			}

			auto inR = [&](const NkRect &r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
			const NkColor hovBg = lightBar ? NkColor{0, 0, 0, 24} : NkColor{255, 255, 255, 26};
			// Compact (Banani) : trois boutons 13×13, écart 4, marge droite 12 —
			// des PIXELS de maquette, pas des unités à passer par S() (même règle
			// que SetHeaderLayout). Historique : trois zones larges de 42.
			const float32 bw = mUI.S(42.f);
			NkRect cClose, cMax, cMin;
			if (mWinControlsCompact) {
				// `mWinControlsSize` : l'application choisit le côté (Rodolf,
				// 31/08 : les 13 px de la maquette étaient trop petits à l'usage).
				const float32 cs = mWinControlsSize, gap = 4.f, right = 12.f;
				const float32 cyBtn = bar.y + (bar.h - cs) * 0.5f;
				cClose = {bar.x + bar.w - right - cs, cyBtn, cs, cs};
				cMax = {cClose.x - gap - cs, cyBtn, cs, cs};
				cMin = {cMax.x - gap - cs, cyBtn, cs, cs};
			} else {
				cClose = {bar.x + bar.w - bw, bar.y, bw, bar.h};
				cMax = {bar.x + bar.w - bw * 2.f, bar.y, bw, bar.h};
				cMin = {bar.x + bar.w - bw * 3.f, bar.y, bw, bar.h};
			}

			// Menus DANS la barre de titre (uniquement dans l'editeur, pas le launcher).
			if (!mUI.appFullScreen)
				BuildMenuBar(ec, {menuX, bar.y, cMin.x - menuX, bar.h});
			else
				mUI.menuBarX = menuX; // pour les zones de drag

			// Infos specifiques au centre (ex. fichier actif) = "panneau du milieu".
			// On retient son rect [titleLx, titleRx] pour delimiter les zones de drag.
			float32 titleLx = bar.x + bar.w, titleRx = bar.x + bar.w; // vide par defaut
			const char *info = mTitleCenter[0] ? mTitleCenter : mTitle;
			if (mUI.font && mUI.font->Face() && info[0] && !mUI.appFullScreen) {
				const float32 iw = mUI.font->MeasureWidth(info);
				const float32 ix = bar.x + (bar.w - iw) * 0.5f;
				titleLx = ix - mUI.S(10.f);
				titleRx = ix + iw + mUI.S(10.f); // + petite marge
				const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
				// Costume (police de barre posée) : la pastille « ● » d'un fichier
				// non enregistré se peint en `theme.warning` et le NOM en
				// `theme.text` — Banani TopHeader. Historique sinon : gris moyen.
				if (mTitleBarFont && info[0] == '\xE2' && info[1] == '\x97' && info[2] == '\x8F') {
					dl.AddText(mUI.font->Face(), mUI.font->TexId(), {ix, by}, "\xE2\x97\x8F",
							   mUI.theme.warning);
					const float32 dw = mUI.font->MeasureWidth("\xE2\x97\x8F");
					dl.AddText(mUI.font->Face(), mUI.font->TexId(), {ix + dw, by}, info + 3,
							   mUI.theme.text);
				} else if (mTitleBarFont)
					dl.AddText(mUI.font->Face(), mUI.font->TexId(), {ix, by}, info, mUI.theme.text);
				else
					dl.AddText(mUI.font->Face(), mUI.font->TexId(), {ix, by}, info, {150, 150, 150, 255});
			}

			bool consumed = false;
			// Chip arrondi inset (style design) pour le fond de survol des controles.
			auto chip = [&](const NkRect &r) -> NkRect {
				const float32 vy = mUI.S(5.f), hx = mUI.S(3.f);
				return {r.x + hx, r.y + vy, r.w - 2.f * hx, r.h - 2.f * vy};
			};
			const float32 cround = mUI.S(4.f);
			// ── Compact 13×13 (Banani TopHeader) : réduire/agrandir sur fond
			//    `theme.button`, glyphes 7 px `textDisabled`, fermer FOND ROUGE
			//    PERMANENT #f85149 avec × blanc. Les trois se dessinent ici et les
			//    blocs historiques sont sautés. ──
			if (mWinControlsCompact) {
				// Les GLYPHES SUIVENT LE CÔTÉ choisi (proportions de la maquette
				// à 13 px, mises à l'échelle) — agrandir la zone cliquable sans
				// agrandir le dessin aurait fait des boutons vides.
				const float32 cs2 = mWinControlsSize;
				const float32 demiTrait = cs2 * 0.27f; // 3.5/13
				const float32 carre = cs2 * 0.46f;	   // 6/13
				const float32 demiX = cs2 * 0.19f;
				// Réduire.
				{
					const bool h = inR(cMin);
					dl.AddRectFilled(cMin, h ? mUI.theme.buttonHover : mUI.theme.button, 3.f);
					const float32 gx = cMin.x + cMin.w * 0.5f, gy = cMin.y + cMin.h * 0.5f;
					dl.AddLine({gx - demiTrait, gy}, {gx + demiTrait, gy}, mUI.theme.textDisabled,
							   1.2f);
					if (h && mUI.input.mouseClicked[0]) {
						mWindow.Minimize();
						consumed = true;
					}
				}
				// Agrandir / restaurer.
				{
					const bool h = inR(cMax);
					dl.AddRectFilled(cMax, h ? mUI.theme.buttonHover : mUI.theme.button, 3.f);
					const float32 gx = cMax.x + cMax.w * 0.5f, gy = cMax.y + cMax.h * 0.5f;
					dl.AddRect({gx - carre * 0.5f, gy - carre * 0.5f, carre, carre},
							   mUI.theme.textDisabled, 1.2f);
					if (h && mUI.input.mouseClicked[0]) {
						mWindow.Maximize();
						consumed = true;
					}
				}
				// Fermer — rouge permanent (la maquette le montre ainsi au repos).
				{
					const bool h = inR(cClose);
					const NkColor rouge = {248, 81, 73, 255}; // #f85149 (Banani --color-error)
					dl.AddRectFilled(cClose, h ? NkColor{255, 110, 102, 255} : rouge, 3.f);
					const float32 gx = cClose.x + cClose.w * 0.5f, gy = cClose.y + cClose.h * 0.5f;
					const NkColor blanc = {255, 255, 255, 255};
					dl.AddLine({gx - demiX, gy - demiX}, {gx + demiX, gy + demiX}, blanc, 1.4f);
					dl.AddLine({gx - demiX, gy + demiX}, {gx + demiX, gy - demiX}, blanc, 1.4f);
					if (h && mUI.input.mouseClicked[0]) {
						mRunning = false;
						consumed = true;
					}
				}
			}
			// Minimiser (trait).
			if (!mWinControlsCompact) {
				const bool h = inR(cMin);
				if (h)
					dl.AddRectFilled(chip(cMin), hovBg, cround);
				const float32 gx = cMin.x + cMin.w * 0.5f;
				dl.AddLine({gx - mUI.S(5.f), cy}, {gx + mUI.S(5.f), cy}, fg, 1.f);
				if (h && mUI.input.mouseClicked[0]) {
					mWindow.Minimize();
					consumed = true;
				}
			}
			// Maximiser / restaurer (carre, ou double carre si maximise).
			if (!mWinControlsCompact) {
				const bool h = inR(cMax);
				if (h)
					dl.AddRectFilled(chip(cMax), hovBg, cround);
				const float32 gx = cMax.x + cMax.w * 0.5f, s = mUI.S(9.f);
				if (mWindow.IsMaximized()) {
					dl.AddRect({gx - s * 0.5f + 2.f, cy - s * 0.5f - 2.f, s - 2.f, s - 2.f}, fg, 1.f);
					dl.AddRectFilled({gx - s * 0.5f - 2.f, cy - s * 0.5f + 2.f, s - 2.f, s - 2.f}, bg);
					dl.AddRect({gx - s * 0.5f - 2.f, cy - s * 0.5f + 2.f, s - 2.f, s - 2.f}, fg, 1.f);
				} else {
					dl.AddRect({gx - s * 0.5f, cy - s * 0.5f, s, s}, fg, 1.f);
				}
				if (h && mUI.input.mouseClicked[0]) {
					mWindow.Maximize();
					consumed = true;
				}
			}
			// Fermer (X, survol rouge #f85149 arrondi).
			if (!mWinControlsCompact) {
				const bool h = inR(cClose);
				if (h)
					dl.AddRectFilled(chip(cClose), {248, 81, 73, 255}, cround);
				const NkColor xc = h ? NkColor{255, 255, 255, 255} : fg;
				const float32 gx = cClose.x + cClose.w * 0.5f, s = mUI.S(5.f);
				dl.AddLine({gx - s, cy - s}, {gx + s, cy + s}, xc, 1.2f);
				dl.AddLine({gx - s, cy + s}, {gx + s, cy - s}, xc, 1.2f);
				if (h && mUI.input.mouseClicked[0]) {
					mRunning = false;
					consumed = true;
				}
			}

			// Glissement de fenetre facon VSCode : DEUX zones de deplacement, dans les
			// GAPS uniquement -> (1) apres le dernier menu et avant le panneau central,
			// (2) apres le panneau central et avant les controles de fenetre. Les menus
			// et le panneau central ne sont JAMAIS des zones de drag. En plus, le
			// deplacement ne demarre qu'apres un vrai GLISSEMENT (seuil) : un simple clic
			// ne deplace jamais la fenetre.
			const bool inBarY = (m.y >= bar.y && m.y < bar.y + bar.h);
			const float32 gap1L = mUI.menuBarX + 2.f, gap1R = titleLx; // menus -> centre
			const float32 gap2L = titleRx, gap2R = cMin.x;			   // centre -> controles
			const bool inGap1 = inBarY && m.x >= gap1L && m.x < gap1R;
			const bool inGap2 = inBarY && m.x >= gap2L && m.x < gap2R;
			const bool dragArea = inGap1 || inGap2;
			if (!consumed && dragArea && mUI.input.mouseDoubleClicked[0]) {
				mWindow.Maximize();
				mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false;
				mTitleDragArmed = false;
			} else if (!consumed && dragArea && mUI.input.mouseClicked[0]) {
				mTitleDragArmed = true;
				mDragStartX = m.x;
				mDragStartY = m.y; // arme, sans deplacer encore
			}
			if (mTitleDragArmed) {
				if (!mUI.input.mouseDown[0]) {
					mTitleDragArmed = false; // relache sans bouger = simple clic
				} else {
					const float32 dx = m.x - mDragStartX, dy = m.y - mDragStartY;
					if (dx * dx + dy * dy > 25.f) { // > ~5 px = vrai glissement
						mTitleDragArmed = false;
						mPendingDragMove = true; // hand-off natif en fin de boucle Run() (anti re-entrance)
						mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false;
					}
				}
			}
			mUI.font = fontInterface; // fin de barre : la police d'interface reprend
		}

		// ── Barre d'outils horizontale (sous la barre de titre, facon Visual Studio) ─
		void NkEditorShell::DrawToolbar(NkEditorFrameContext &ec, const NkRect &rect) noexcept {
			auto &dl = mUI.dl;
			dl.AddRectFilled(rect, {22, 26, 32, 255});										   // gris sombre #161A20
			dl.AddRectFilled({rect.x, rect.y + rect.h - 1.f, rect.w, 1.f}, {40, 45, 53, 255}); // separateur
			if (!mToolbarFn)
				return;
			// Region de layout horizontale : l'app pose Button/Combo + SameLine().
			const float32 itemH = mUI.ItemHeight();
			mUI.layout.region = rect;
			mUI.layout.cursor = {rect.x + mUI.S(8.f), rect.y + (rect.h - itemH) * 0.5f};
			mUI.layout.lineStartX = mUI.layout.cursor.x;
			mUI.layout.curLineH = 0.f;
			mUI.layout.maxX = mUI.layout.cursor.x;
			mToolbarFn(ec, mToolbarUser);
		}

		// ── Bords de redimensionnement (fenetre sans bordure) ─────────────────────
		// Redimensionnement MANUEL par les bords. Le resize natif (WM_NCLBUTTONDOWN/
		// HTLEFT) ne fonctionne pas dans ce setup borderless ; on gere donc tout a la
		// main via GetPosition/GetSize + SetPosition/SetSize, en suivant la souris ECRAN.
		void NkEditorShell::HandleEdgeResize(float32 W, float32 H) noexcept {
			if (mWindow.IsMaximized())
				return;
			const float32 b = mUI.S(7.f);
			const NkVec2 m = mUI.input.mousePos; // coords CLIENT (NCHITTEST=HTCLIENT)
			const bool L = m.x <= b, R = m.x >= W - b, T = m.y <= b, Bm = m.y >= H - b;
			const int32 edge = (L ? 1 : 0) | (R ? 2 : 0) | (T ? 4 : 0) | (Bm ? 8 : 0);
			if (!edge)
				return;

			// Curseur de redimensionnement au survol d'un bord (pas de diagonale dispo).
			if ((edge & 3) && !(edge & 12))
				mUI.wantCursor = NkGuiCursor::ResizeEW; // gauche/droite
			else if ((edge & 12) && !(edge & 3))
				mUI.wantCursor = NkGuiCursor::ResizeNS; // haut/bas
			else
				mUI.wantCursor = NkGuiCursor::ResizeEW; // coin

			// Clic sur un bord -> HAND-OFF NATIF a l'OS (resize fluide + aero-snap), sans
			// contournement : chaque backend implemente NkWindow::BeginResize nativement.
			if (mUI.input.mouseClicked[0]) {
				NkWindow::NkResizeEdge e = NkWindow::NkResizeEdge::Left;
				switch (edge) {
					case 1:
						e = NkWindow::NkResizeEdge::Left;
						break;
					case 2:
						e = NkWindow::NkResizeEdge::Right;
						break;
					case 4:
						e = NkWindow::NkResizeEdge::Top;
						break;
					case 8:
						e = NkWindow::NkResizeEdge::Bottom;
						break;
					case 1 | 4:
						e = NkWindow::NkResizeEdge::TopLeft;
						break;
					case 2 | 4:
						e = NkWindow::NkResizeEdge::TopRight;
						break;
					case 1 | 8:
						e = NkWindow::NkResizeEdge::BottomLeft;
						break;
					case 2 | 8:
						e = NkWindow::NkResizeEdge::BottomRight;
						break;
					default:
						return;
				}
				mUI.input.mouseClicked[0] = false;			// consomme (pas de drag de barre de titre)
				mTitleDragArmed = false;					// annule un eventuel arme de drag titre
				mPendingResizeEdge = static_cast<int32>(e); // declenche le hand-off natif en fin de boucle Run()
			}
		}

		// ── Barre d'etat (footer facon VSCode : bande bleue en bas) ───────────────
		void NkEditorShell::DrawStatusBar(float32 footerH) noexcept {
			const float32 W = static_cast<float32>(mUI.viewW);
			const float32 H = static_cast<float32>(mUI.viewH);
			const NkRect bar = {0.f, H - footerH, W, footerH};
			mUI.dl.AddRectFilled(bar, mUI.theme.header);				  // status bar (suit Dark/Light)
			mUI.dl.AddRectFilled({0.f, bar.y, W, 1.f}, mUI.theme.border); // liseret haut
			if (!mUI.font || !mUI.font->Face())
				return;
			const NkColor fg = mUI.theme.text; // texte theme
			const float32 pad = mUI.S(10.f);
			const float32 by = bar.y + (footerH - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
			float32 leftX = bar.x + pad;
			// ── VOYANTS (petites pastilles SANS texte, poussees par l'app via
			// SetFooterLights : etat sante code/compilation/link...) ; le libelle
			// n'apparait qu'en INFOBULLE au survol. ──
			for (int32 i = 0; i < mFooterLightCount; ++i) {
				const float32 r = mUI.S(4.f);
				const NkVec2 c = {leftX + r, bar.y + footerH * 0.5f};
				mUI.dl.AddCircleFilled(c, r, mFooterLights[i]);
				const NkRect hit = {c.x - r - 2.f, c.y - r - 2.f, r * 2.f + 4.f, r * 2.f + 4.f};
				if (!mFooterLightTips[i].Empty())
					NkTooltip(mUI, nkgui::NkGuiRectContains(hit, mUI.input.mousePos),
							  mFooterLightTips[i].CStr());
				leftX += r * 2.f + mUI.S(6.f);
			}
			if (mFooterLightCount > 0)
				leftX += pad * 0.5f;
			if (mFooterLeft[0])
				mUI.dl.AddText(mUI.font->Face(), mUI.font->TexId(), {leftX, by}, mFooterLeft, fg);
			float32 rightX = bar.x + W - pad; // bord droit courant (les elements s'empilent vers la gauche)
			if (mFooterRight[0]) {
				const float32 rw = mUI.font->MeasureWidth(mFooterRight);
				mUI.dl.AddText(mUI.font->Face(), mUI.font->TexId(), {rightX - rw, by}, mFooterRight, fg);
				rightX -= rw + pad * 2.f;
			}
			// Indicateur de ZOOM (police du code) : "Zoom NNN%" cliquable -> reinitialise (Ctrl+0).
			// ⚠️ C'est le zoom de la POLICE DE CODE, pas un zoom de vue : une
			//    application sans editeur de code le masque (SetFooterZoomIndicator).
			if (mFooterZoom) {
				const int32 pct = static_cast<int32>(ActiveCodeSize() / kDefaultCodeFontSize * 100.f + 0.5f);
				char z[24];
				std::snprintf(z, sizeof(z), "Zoom %d%%", pct);
				const float32 zw = mUI.font->MeasureWidth(z);
				const NkRect zr = {rightX - zw - pad, bar.y + 2.f, zw + pad * 2.f, footerH - 3.f};
				const bool zhov = nkgui::NkGuiRectContains(zr, mUI.input.mousePos);
				if (zhov)
					mUI.dl.AddRectFilled(zr, mUI.theme.buttonHover, 3.f);
				mUI.dl.AddText(mUI.font->Face(), mUI.font->TexId(), {zr.x + pad, by}, z,
							   (pct != 100 || zhov) ? fg : mUI.theme.textDisabled);
				if (zhov) {
					mUI.wantCursor = nkgui::NkGuiCursor::Hand;
					if (mUI.input.mouseClicked[0])
						ResetCodeFontSize();
				}
			}
		}

		// Construit la combinaison "Ctrl+[Shift+]X" depuis la touche et l'execute
		// si un raccourci enregistre correspond. NkKeyToString -> "NK_X".
		void NkEditorShell::TryRunShortcut(NkKey k, bool shift) noexcept {
			const char *kn = NkKeyToString(k);
			if (!kn || kn[0] != 'N' || kn[1] != 'K' || kn[2] != '_' || !kn[3] || kn[4])
				return; // exige "NK_X"
			char combo[24];
			usize n = 0;
			for (const char *p = "Ctrl+"; *p; ++p)
				combo[n++] = *p;
			if (shift)
				for (const char *p = "Shift+"; *p; ++p)
					combo[n++] = *p;
			combo[n++] = kn[3];
			combo[n] = '\0';
			for (int32 i = 0; i < mNumCommands; ++i) {
				const char *s = mCommands[i].shortcut;
				usize j = 0;
				bool eq = true;
				for (; s[j] && combo[j]; ++j)
					if (s[j] != combo[j]) {
						eq = false;
						break;
					}
				if (eq && !s[j] && !combo[j]) {
					ExecuteCommand(i);
					return;
				}
			}
		}

		bool NkEditorShell::FocusPanel(const char *title) noexcept {
			if (!title)
				return false;
			for (int32 i = 0; i < mNumPanels; ++i) {
				NkEditorPanel *p = mPanels[i];
				if (!p)
					continue;
				const char *a = p->Title();
				const char *b = title;
				while (*a && *a == *b) {
					++a;
					++b;
				}
				if (*a || *b)
					continue;
				p->SetOpen(true); // fenetre fermee (etat persiste) -> l'OUVRIR d'abord, sinon rien a focus
				// JAMAIS ancrée (fermée au moment du bootstrap) -> l'ancrer à son côté PAR
				// DÉFAUT au lieu d'apparaître flottante : en onglet avec un panneau du même
				// côté déjà ancré si possible, sinon nouvelle zone de bord. No-op si déjà
				// ancrée -> une place choisie à la main (drag) est conservée.
				if (p->Dockable() && !DockIsWindowDocked(mUI, title)) {
					const NkEditorDockSide side = p->DefaultSide();
					// Une feuille n'accueille le panneau que si elle est vraiment « la
					// sidebar » du côté : pour gauche/droite elle ne doit contenir QUE des
					// panneaux de ce côté — un panneau du groupe DÉPLACÉ dans la zone du
					// terminal est indépendant et ne doit pas aspirer les ouvertures.
					auto leafOfSide = [&](const char *qt) -> bool {
						const int32 node = DockWindowNode(mUI, qt);
						if (node < 0 || node >= static_cast<int32>(mUI.dockNodes.Size()))
							return false;
						if (side != NkEditorDockSide::NK_LEFT && side != NkEditorDockSide::NK_RIGHT)
							return true; // bas/centre : regroupement historique
						const NkGuiDockNode &L = mUI.dockNodes[node];
						for (int32 w = 0; w < L.winCount; ++w) {
							bool ok = false;
							for (int32 j = 0; j < mNumPanels; ++j)
								if (mPanels[j] && mUI.GetId(mPanels[j]->Title()) == L.windows[w]) {
									ok = (mPanels[j]->DefaultSide() == side);
									break;
								}
							if (!ok)
								return false;
						}
						return true;
					};
					const char *host = nullptr;
					for (int32 j = 0; j < mNumPanels; ++j) {
						NkEditorPanel *q = mPanels[j];
						if (q && q != p && q->Dockable() && q->DefaultSide() == side &&
							DockIsWindowDocked(mUI, q->Title()) && leafOfSide(q->Title())) {
							host = q->Title();
							break;
						}
					}
					if (host)
						DockBuilderDockTab(mUI, title, host);
					else
						DockBuilderDock(mUI, title, SideToZone(side));
				}
				DockFocusWindow(mUI, title);
				return true;
			}
			return false;
		}

		static bool SameTitle(const char *a, const char *b) {
			if (!a || !b)
				return false;
			while (*a && *a == *b) {
				++a;
				++b;
			}
			return *a == *b;
		}

		bool NkEditorShell::IsPanelOpen(const char *title) noexcept {
			for (int32 i = 0; i < mNumPanels; ++i)
				if (mPanels[i] && SameTitle(mPanels[i]->Title(), title))
					return mPanels[i]->IsOpen();
			return false;
		}

		void NkEditorShell::ClosePanel(const char *title) noexcept {
			for (int32 i = 0; i < mNumPanels; ++i)
				if (mPanels[i] && SameTitle(mPanels[i]->Title(), title)) {
					mPanels[i]->SetOpen(false);
					return;
				}
		}

		// Ajuste le ratio du split PARENT de la feuille contenant `title` -> la region
		// (ex. panneau du bas + ses onglets) prend presque tout (maximise), juste les
		// onglets (replie), ou revient au ratio normal (restaure).
		void NkEditorShell::SetRegionMode(const char *title, int32 mode) noexcept {
			const int32 leaf = DockWindowNode(mUI, title);
			if (leaf < 0 || leaf >= static_cast<int32>(mUI.dockNodes.Size()))
				return;
			const int32 par = mUI.dockNodes[leaf].parent;
			if (par < 0 || par >= static_cast<int32>(mUI.dockNodes.Size()))
				return;
			nkgui::NkGuiDockNode &split = mUI.dockNodes[par];
			if (split.kind != 1 || split.vertical)
				return; // il faut un split HAUT|BAS
			const bool bottomChild1 = (split.child1 == leaf);
			if (mDockRegionState == 0 && mode != 0)
				mDockSavedRatio = split.ratio; // sauvegarde le ratio normal une seule fois
			const float32 h = split.rect.h > 1.f ? split.rect.h : 400.f;
			float32 minFrac = (mUI.ItemHeight() + 6.f) / h; // hauteur minimale (onglets visibles)
			if (minFrac > 0.4f)
				minFrac = 0.4f;
			if (mode == 0) {
				if (mDockSavedRatio >= 0.f)
					split.ratio = mDockSavedRatio;
			} else if (mode == 1) // replie : la region = juste ses onglets
				split.ratio = bottomChild1 ? (1.f - minFrac) : minFrac;
			else // maximise : la region prend presque toute la hauteur
				split.ratio = bottomChild1 ? minFrac : (1.f - minFrac);
			mDockRegionState = mode;
		}

		void NkEditorShell::ToggleMaximizePanel(const char *title) noexcept {
			SetRegionMode(title, mDockRegionState == 2 ? 0 : 2);
		}
		void NkEditorShell::ToggleCollapsePanel(const char *title) noexcept {
			SetRegionMode(title, mDockRegionState == 1 ? 0 : 1);
		}

		bool NkEditorShell::IsBottomRegionTab(NkGuiContext &c, NkGuiId win) noexcept {
			const char *title = nullptr;
			for (int32 i = 0; i < mNumPanels; ++i)
				if (mPanels[i] && c.GetId(mPanels[i]->Title()) == win) {
					title = mPanels[i]->Title();
					break;
				}
			if (!title)
				return false;
			const int32 leaf = DockWindowNode(c, title);
			if (leaf < 0 || leaf >= static_cast<int32>(c.dockNodes.Size()))
				return false;
			const int32 par = c.dockNodes[leaf].parent;
			return par >= 0 && par < static_cast<int32>(c.dockNodes.Size()) && c.dockNodes[par].kind == 1 &&
				   !c.dockNodes[par].vertical; // split HAUT|BAS
		}

		// Boutons maximiser/replier de la REGION, dessines par le shell -> dispo pour TOUS les onglets.
		void NkEditorShell::DrawRegionButtons(NkGuiContext &c, const NkRect &area, NkGuiId win) noexcept {
			const char *title = nullptr;
			for (int32 i = 0; i < mNumPanels; ++i)
				if (mPanels[i] && c.GetId(mPanels[i]->Title()) == win) {
					title = mPanels[i]->Title();
					break;
				}
			if (!title)
				return;
			auto &dl = c.DL();
			const float32 bh2 = area.h - 8.f;
			const NkVec2 m = c.input.mousePos;
			const int32 rmode = mDockRegionState;
			auto inR = [&](const NkRect &r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
			// Maximiser / restaurer (a l'extreme droite).
			const NkRect maxR = {area.x + area.w - bh2 - 4.f, area.y + 4.f, bh2, bh2};
			const bool mh = inR(maxR);
			if (mh)
				dl.AddRectFilled(maxR, c.theme.buttonHover, 3.f);
			{
				const NkColor col = mh ? c.theme.text : c.theme.textDisabled;
				const float32 s2 = bh2 - 12.f;
				if (rmode == 2) {
					dl.AddRect({maxR.x + 5.f, maxR.y + 7.f, s2, s2}, col, 1.4f);
					dl.AddRect({maxR.x + 8.f, maxR.y + 4.f, s2, s2}, col, 1.4f);
				} else {
					const NkRect w = {maxR.x + 5.f, maxR.y + 5.f, bh2 - 10.f, bh2 - 10.f};
					dl.AddRect(w, col, 1.4f);
					dl.AddRectFilled({w.x, w.y, w.w, 2.5f}, col);
				}
				if (mh && c.input.mouseClicked[0] && c.popupDepth == 0)
					SetRegionMode(title, mDockRegionState == 2 ? 0 : 2);
			}
			// Replier / restaurer (chevron, a gauche du maximiser).
			const NkRect colR = {maxR.x - bh2 - 4.f, area.y + 4.f, bh2, bh2};
			const bool ch2 = inR(colR);
			if (ch2)
				dl.AddRectFilled(colR, c.theme.buttonHover, 3.f);
			{
				const NkColor col = ch2 ? c.theme.text : c.theme.textDisabled;
				const float32 cx = colR.x + bh2 * 0.5f, cy = colR.y + bh2 * 0.5f;
				if (rmode == 1) { // replie -> chevron HAUT (restaurer)
					dl.AddLine({cx - 4.f, cy + 2.f}, {cx, cy - 2.f}, col, 1.6f);
					dl.AddLine({cx, cy - 2.f}, {cx + 4.f, cy + 2.f}, col, 1.6f);
				} else { // etendu -> chevron BAS (replier)
					dl.AddLine({cx - 4.f, cy - 2.f}, {cx, cy + 2.f}, col, 1.6f);
					dl.AddLine({cx, cy + 2.f}, {cx + 4.f, cy - 2.f}, col, 1.6f);
				}
				if (ch2 && c.input.mouseClicked[0] && c.popupDepth == 0)
					SetRegionMode(title, mDockRegionState == 1 ? 0 : 1);
			}
		}

		int32 NkEditorShell::PanelDockNode(const char *title) noexcept {
			return DockWindowNode(mUI, title);
		}

		void NkEditorShell::DetachPanel(const char *title) noexcept {
			DockDetachWindow(mUI, title);
		}

		void NkEditorShell::SetFooter(const char *left, const char *right) noexcept {
			CopyStr(mFooterLeft, left ? left : "", sizeof(mFooterLeft));
			CopyStr(mFooterRight, right ? right : "", sizeof(mFooterRight));
			// Barre d'etat debranchee : le message va au RAIL BAS — sans ce
			// routage, « gfx ecrit, actif au prochain lancement » deviendrait
			// invisible le jour ou une app fusionne ses bandeaux.
			if (!mStatusBarVisible && !mStatusBarFn) {
				char joint[256];
				const bool deux = right && *right;
				snprintf(joint, sizeof(joint), deux ? "%s%s" : "%s", left ? left : "",
						 deux ? right : "");
				CopyStr(mRailFooterText, joint, sizeof(mRailFooterText));
			}
		}

		void NkEditorShell::SetTitleInfo(const char *center) noexcept {
			CopyStr(mTitleCenter, center ? center : "", sizeof(mTitleCenter));
		}

		// ── Barre de menus ───────────────────────────────────────────────────────
		void NkEditorShell::BuildMenuBar(NkEditorFrameContext &ec, const NkRect &rect) noexcept {
			if (!BeginMenuBar(mUI, rect))
				return;

			// Sur l'ecran de demarrage (launcher), PAS de menus (Fichier, etc.) : on
			// appelle quand meme mAppMenuFn (il pose les flags appFullScreen/appModal
			// chaque frame -> indispensable au maintien du launcher).
			if (mUI.appFullScreen) {
				if (mAppMenuFn)
					mAppMenuFn(ec, mAppMenuUser);
				EndMenuBar(mUI);
				return;
			}

			// Barre COMPLETE fournie par l'app (SetMenuBar) : remplace entierement
			// les menus par defaut ci-dessous (spec barre de menus NKCode/Banani).
			if (mMenuBarFn) {
				mMenuBarFn(ec, mMenuBarUser);
				if (mAppMenuFn)
					mAppMenuFn(ec, mAppMenuUser); // flags launcher (appFullScreen/appModal)
				EndMenuBar(mUI);
				return;
			}

			if (BeginMenu(mUI, "Fichier")) {
				if (mFileMenuFn)
					mFileMenuFn(ec, mFileMenuUser); // Nouveau/Enregistrer/Deploiement (app)
				if (MenuItem(mUI, "Palette de commandes", "Ctrl+P")) {
					mPaletteOpen = !mPaletteOpen;
					mPaletteSel = 0;
				}
				if (MenuItem(mUI, "Quitter", "Alt+F4"))
					mRunning = false;
				EndMenu(mUI);
			}
			if (BeginMenu(mUI, "Affichage")) {
				DrawPanelsMenuItems();
				EndMenu(mUI);
			}
			if (BeginMenu(mUI, "Fenetre")) {
				if (MenuItem(mUI, "Reinitialiser la disposition"))
					ResetLayout();
				EndMenu(mUI);
			}
			// Menu dedie aux reglages (extensible : polices, theme, et plus a venir).
			if (BeginMenu(mUI, "Preferences")) {
				if (MenuItem(mUI, "Polices..."))
					OpenPreferences(0);
				if (MenuItem(mUI, "Theme..."))
					OpenPreferences(1);
				if (MenuItem(mUI, "Langages..."))
					OpenPreferences(2);
				EndMenu(mUI);
			}
			if (mAppMenuFn)
				mAppMenuFn(ec, mAppMenuUser);

			EndMenuBar(mUI);
		}

		// Boucle « un item par panneau » (ouvrir = FocusPanel pour ANCRER au cote
		// par defaut ; deja ouvert = fermer) — partagee entre le menu Affichage
		// historique et la barre fournie par l'app (Open View).
		void NkEditorShell::DrawPanelsMenuItems() noexcept {
			for (int32 i = 0; i < mNumPanels; ++i)
				if (MenuItem(mUI, mPanels[i]->Title())) {
					if (mPanels[i]->IsOpen())
						mPanels[i]->SetOpen(false);
					else
						FocusPanel(mPanels[i]->Title());
				}
		}

		// ── Etat d'interface par projet (maximise + panneaux ouverts) ────────────
		void NkEditorShell::LoadUiState(const char *path) noexcept {
			if (!path || !*path)
				return;
			const NkString txt = NkFile::ReadAllText(NkPath(path));
			if (txt.Empty())
				return; // pas de config -> on garde l'etat courant
			// Parse ligne par ligne : "maximized=0/1" et "panel=<titre>".
			bool sawPanel = false;
			// 1re passe : si des lignes panel= existent, on ferme tout avant d'ouvrir.
			const char *p = txt.CStr();
			for (const char *q = p; *q; ++q)
				if (q[0] == 'p' && q[1] == 'a' && q[2] == 'n' && q[3] == 'e' && q[4] == 'l') {
					sawPanel = true;
					break;
				}
			if (sawPanel)
				for (int32 i = 0; i < mNumPanels; ++i)
					mPanels[i]->SetOpen(false);

			NkString line;
			bool dockRestored = false;
			// Meta de fenêtre par id — créée au besoin (titre copié : les onglets du dock
			// l'affichent avant le premier Begin de la fenêtre).
			auto ensureMeta = [&](NkGuiId wid, const char *tt) -> NkGuiWindowMeta * {
				for (uint32 i = 0; i < mUI.windowMeta.Size(); ++i)
					if (mUI.windowMeta[i].id == wid)
						return &mUI.windowMeta[i];
				NkGuiWindowMeta nm;
				nm.id = wid;
				int32 k = 0;
				for (; tt[k] && k < 47; ++k)
					nm.title[k] = tt[k];
				nm.title[k] = 0;
				mUI.windowMeta.PushBack(nm);
				return &mUI.windowMeta[mUI.windowMeta.Size() - 1];
			};
			auto apply = [&](const NkString &ln) {
				const char *s = ln.CStr();
				if (StartsWith(s, "win=")) {
					// Restaure la TAILLE seulement (pas la position -> pas de changement de
					// moniteur/DPI qui désynchroniserait l'échelle). Le resize du renderer
					// suit dans la boucle (GetSize).
					int32 x = 0, y = 0, w = 0, h = 0;
					if (std::sscanf(s + 4, "%d|%d|%d|%d", &x, &y, &w, &h) == 4 && w > 200 && h > 150 && w < 20000 &&
						h < 20000) {
						if (mWindow.IsMaximized())
							mWindow.Restore(); // sortir de l'état maximisé AVANT de dimensionner
						mWindow.SetSize(static_cast<uint32>(w), static_cast<uint32>(h));
					}
				} else if (StartsWith(s, "maximized=")) {
					// Maximize()/Restore() du moteur BASCULENT (toggle) -> guarder par
					// l'état courant, sinon un launcher déjà maximisé serait dé-maximisé.
					if (s[10] == '1') {
						if (!mWindow.IsMaximized())
							mWindow.Maximize();
					} else if (mWindow.IsMaximized())
						mWindow.Restore();
				} else if (StartsWith(s, "panel=")) {
					const char *name = s + 6;
					for (int32 i = 0; i < mNumPanels; ++i)
						if (StrEqual(mPanels[i]->Title(), name)) {
							mPanels[i]->SetOpen(true);
							break;
						}
				} else if (StartsWith(s, "dockroot=")) {
					// Disposition sérialisée -> RESET du dock courant (l'arbre du fichier
					// fait foi) ; les nœuds arrivent ensuite dans l'ordre 0..n-1 (DFS).
					mUI.dockNodes.Clear();
					mUI.dockRoot = -1;
					for (uint32 i = 0; i < mUI.windowMeta.Size(); ++i) {
						mUI.windowMeta[i].dockNode = -1;
						mUI.windowMeta[i].hostRoot = -1;
						mUI.windowMeta[i].dockHost = NKGUI_ID_NONE;
						mUI.windowMeta[i].dockActiveTab = false;
					}
					dockRestored = true;
				} else if (dockRestored && StartsWith(s, "node=")) {
					int32 idx = 0, kind = 0, vert = 0, c0 = -1, c1 = -1, at = 0;
					float32 ratio = 0.5f;
					if (std::sscanf(s + 5, "%d|%d|%d|%f|%d|%d|%d", &idx, &kind, &vert, &ratio, &c0, &c1, &at) == 7) {
						NkGuiDockNode nd;
						nd.kind = static_cast<uint8>(kind);
						nd.vertical = vert != 0;
						nd.ratio = ratio;
						nd.child0 = c0;
						nd.child1 = c1;
						nd.activeTab = at;
						mUI.dockNodes.PushBack(nd);
					}
				} else if (dockRestored && StartsWith(s, "nwin=")) {
					const char *b = s + 5;
					int32 idx = 0;
					while (*b >= '0' && *b <= '9')
						idx = idx * 10 + (*b++ - '0');
					if (*b == '|' && b[1] && idx >= 0 && idx < static_cast<int32>(mUI.dockNodes.Size())) {
						NkGuiDockNode &L = mUI.dockNodes[idx];
						if (L.kind == 2 && L.winCount < 8) {
							const NkGuiId wid = mUI.GetId(b + 1);
							L.windows[L.winCount++] = wid;
							ensureMeta(wid, b + 1)->dockNode = idx;
						}
					}
				} else if (dockRestored && StartsWith(s, "float=")) {
					float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
					if (std::sscanf(s + 6, "%f|%f|%f|%f", &x, &y, &w, &h) == 4) {
						const char *b = s + 6;
						for (int32 bars = 0; *b && bars < 4; ++b) // titre = après le 4e '|'
							if (*b == '|')
								++bars;
						if (*b && w > 40.f && h > 40.f) {
							NkGuiWindowMeta *wm = ensureMeta(mUI.GetId(b), b);
							wm->rect = {x, y, w, h};
							wm->floatRect = wm->rect;
							wm->init = true;
						}
					}
				}
			};
			for (const char *c = p;; ++c) {
				if (*c == '\n' || *c == '\r' || *c == '\0') {
					if (!line.Empty()) {
						apply(line);
						line.Clear();
					}
					if (*c == '\0')
						break;
				} else
					line += *c;
			}
			// ── Finalisation de la disposition restaurée : parents, drapeaux, filets. ──
			if (dockRestored && mUI.dockNodes.Size() > 0) {
				const int32 n = static_cast<int32>(mUI.dockNodes.Size());
				for (int32 i = 0; i < n; ++i) {
					NkGuiDockNode &d = mUI.dockNodes[i];
					if (d.kind == 1) {
						if (d.child0 < 0 || d.child0 >= n || d.child1 < 0 || d.child1 >= n) {
							d.kind = 2; // arbre corrompu -> feuille vide (robustesse)
							d.child0 = d.child1 = -1;
						} else {
							mUI.dockNodes[d.child0].parent = i;
							mUI.dockNodes[d.child1].parent = i;
						}
					}
					if (d.kind == 2 && d.activeTab >= d.winCount)
						d.activeTab = d.winCount > 0 ? d.winCount - 1 : 0;
				}
				mUI.dockNodes[0].parent = -1;
				mUI.dockRoot = 0;
				for (int32 i = 0; i < n; ++i) { // drapeau « onglet actif » des metas
					const NkGuiDockNode &d = mUI.dockNodes[i];
					if (d.kind != 2)
						continue;
					for (int32 w = 0; w < d.winCount; ++w)
						for (uint32 m2 = 0; m2 < mUI.windowMeta.Size(); ++m2)
							if (mUI.windowMeta[m2].id == d.windows[w]) {
								mUI.windowMeta[m2].dockActiveTab = (w == d.activeTab);
								break;
							}
				}
				mDockBootstrap = false; // la disposition restaurée fait foi
				// Filet : panneau OUVERT dockable jamais mentionné dans le fichier (ex.
				// panneau ajouté depuis) -> ancré à son côté par défaut, pas flottant.
				for (int32 i = 0; i < mNumPanels; ++i) {
					NkEditorPanel *pl = mPanels[i];
					if (!pl->IsOpen() || !pl->Dockable())
						continue;
					const NkGuiId wid = mUI.GetId(pl->Title());
					bool has = false;
					for (uint32 m2 = 0; m2 < mUI.windowMeta.Size() && !has; ++m2)
						if (mUI.windowMeta[m2].id == wid)
							has = true;
					if (!has)
						DockBuilderDock(mUI, pl->Title(), SideToZone(pl->DefaultSide()));
				}
			}
		}

		// ── Gestionnaire de menu contextuel (réutilisable) ───────────────────────
		void NkEditorShell::OpenContextMenu(const nkgui::NkVec2 &pos, const char *const *items, const bool *enabled,
											int32 count) noexcept {
			mCtxItems.Clear();
			mCtxEnabled.Clear();
			for (int32 i = 0; i < count; ++i) {
				mCtxItems.PushBack(NkString(items[i] ? items[i] : ""));
				mCtxEnabled.PushBack(enabled ? (enabled[i] ? 1u : 0u) : 1u);
			}
			mCtxPos = pos;
			mCtxOpen = true;
			mCtxChoice = -1;
			mCtxSy = 0.f;
			mCtxSubItem = -1; // pas de sous-menu tant que SetContextSubmenu n'est pas appelé
			mCtxSubItems.Clear();
			mCtxSub = NkCtxMenu{};
			mCtxSubChoice = -1;
		}

		// Dessiné APRÈS les panneaux (input réel restauré) sur la couche overlay :
		// occlusion propre (les panneaux ont vu un input neutralisé pendant leur draw).
		void NkEditorShell::DrawContextMenu() noexcept {
			if (!mCtxOpen)
				return;
			auto &dl = mUI.dlOverlay;
			auto &in = mUI.input;
			const NkVec2 m = in.mousePos;
			const int32 n = static_cast<int32>(mCtxItems.Size());
			const float32 lh = (mUI.font && mUI.font->Valid()) ? mUI.font->LineHeight() : 16.f;
			const float32 asc = (mUI.font && mUI.font->Valid()) ? mUI.font->Ascent() : 12.f;
			const float32 rowH = lh + 8.f, pad = 12.f;
			// Largeur = plus long libellé.
			float32 w = 120.f;
			if (mUI.font && mUI.font->Valid())
				for (int32 i = 0; i < n; ++i) {
					const float32 tw = mUI.font->MeasureWidth(mCtxItems[i].CStr()) + pad * 2.f + 14.f;
					if (tw > w)
						w = tw;
				}
			const float32 vh = static_cast<float32>(mUI.viewH);
			const float32 fullH = n * rowH + 8.f;
			const float32 maxH = vh * 0.7f;
			const float32 h = fullH > maxH ? maxH : fullH;
			// Reste à l'écran.
			float32 x = mCtxPos.x, y = mCtxPos.y;
			if (x + w > mUI.viewW)
				x = mUI.viewW - w - 4.f;
			if (y + h > vh)
				y = vh - h - 4.f;
			if (x < 2.f)
				x = 2.f;
			if (y < 2.f)
				y = 2.f;
			const NkRect box = {x, y, w, h};
			dl.AddRectFilled({box.x + 2.f, box.y + 3.f, box.w, box.h}, NkColor{0, 0, 0, 60}, 6.f); // ombre
			dl.AddRectFilled(box, mUI.theme.panel, 6.f);
			dl.AddRect(box, mUI.theme.border, 1.f);
			const bool scroll = fullH > maxH;
			const float32 maxSy = scroll ? (fullH - h) : 0.f;
			if (scroll) {
				mCtxSy += -in.wheel * rowH;
				if (mCtxSy < 0.f)
					mCtxSy = 0.f;
				if (mCtxSy > maxSy)
					mCtxSy = maxSy;
			}
			dl.PushClipRect({box.x, box.y + 4.f, box.w, box.h - 8.f}, true);
			float32 ry = box.y + 4.f - mCtxSy;
			int32 clicked = -1;
			for (int32 i = 0; i < n; ++i) {
				const NkRect r = {box.x + 3.f, ry, box.w - 6.f, rowH};
				if (ry + rowH >= box.y && ry <= box.y + box.h) {
					const bool en = mCtxEnabled[i] != 0;
					const bool hov = en && m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
					const bool sub = (i == mCtxSubItem && !mCtxSubItems.Empty());
					if (hov) {
						NkColor sel = mUI.theme.selection;
						sel.a = 110;
						dl.AddRectFilled(r, sel, 4.f);
					}
					if (mUI.font && mUI.font->Valid())
						dl.AddText(mUI.font->Face(), mUI.font->TexId(), {r.x + pad, ry + (rowH - lh) * 0.5f + asc},
								   mCtxItems[i].CStr(), en ? mUI.theme.text : mUI.theme.textDisabled);
					if (sub) { // indicateur ▸ + ouverture au SURVOL (sans clic, façon menus natifs)
						const float32 ax2 = r.x + r.w - 11.f, ay2 = ry + rowH * 0.5f;
						dl.AddTriangleFilled({ax2 - 3.f, ay2 - 4.f}, {ax2 - 3.f, ay2 + 4.f}, {ax2 + 3.f, ay2},
											 en ? mUI.theme.text : mUI.theme.textDisabled);
						if (hov && !mCtxSub.open) {
							mCtxSub.open = true;
							mCtxSub.pos = {r.x + r.w - 4.f, ry};
							mCtxSub.sx = 0.f;
							mCtxSub.sy = 0.f;
						}
					} else if (hov && mCtxSub.open) {
						mCtxSub.open = false; // survol d'un AUTRE item : referme le sous-menu
					}
					if (hov && in.mouseClicked[0] && !sub)
						clicked = i; // l'item parent d'un sous-menu n'est PAS cliquable lui-même
				}
				ry += rowH;
			}
			dl.PopClipRect();
			// ── SOUS-MENU : par-dessus le menu (NkCtxMenuDraw : scrollbars V ET H
			// intégrées pour les listes longues / noms de projets larges ; consomme les
			// clics dans sa boîte -> le menu principal ne se referme pas dessous). ──
			if (mCtxSub.open && !mCtxSubItems.Empty()) {
				int32 sn = static_cast<int32>(mCtxSubItems.Size());
				if (sn > 256)
					sn = 256;
				const char *sitems[256];
				bool sen[256];
				for (int32 i = 0; i < sn; ++i) {
					sitems[i] = mCtxSubItems[static_cast<usize>(i)].CStr();
					sen[i] = true;
				}
				uint32 sico[256];
				for (int32 i = 0; i < sn; ++i)
					sico[i] = (static_cast<usize>(i) < mCtxSubIcons.Size()) ? mCtxSubIcons[static_cast<usize>(i)] : 0u;
				// Icones + barre de recherche ancree : une liste de projets se parcourt
				// ici exactement comme dans le combo de la barre d'outils.
				const int32 act = NkCtxMenuDraw(mUI, mCtxSub, sitems, sen, sn, nullptr, nullptr, sico,
												mCtxSubFilter, static_cast<int32>(sizeof(mCtxSubFilter)),
												&mCtxSubFilterFocus);
				if (act >= 0) {
					mCtxChoice = mCtxSubItem; // le choix = (item parent, index du sous-menu)
					mCtxSubChoice = act;
					mCtxOpen = false;
				}
			}
			if (clicked >= 0) {
				mCtxChoice = clicked;
				mCtxOpen = false;
			}
			// Fermeture : clic gauche OU DROIT hors du menu, ou Échap. (Le clic droit
			// hors du menu le referme -> un nouveau clic droit rouvre au nouvel endroit.)
			if (((in.mouseClicked[0] || in.mouseClicked[1]) && !nkgui::NkGuiRectContains(box, m)) ||
				in.KeyPressed(nkgui::NkGuiKey::Escape))
				mCtxOpen = false;
		}

void NkEditorShell::MaximizeWindow() noexcept {
			if (!mWindow.IsMaximized()) // Maximize() bascule -> guarder
				mWindow.Maximize();
		}

		// Géométrie GLOBALE (launcher) : mêmes lignes win=/maximized= que l'UiState.
		void NkEditorShell::SaveWindowGeom(const char *path) noexcept {
			if (!path || !*path)
				return;
			NkPath p(path);
			NkDirectory::CreateRecursive(p.GetParent());
			NkString out;
			const bool gmax = mGeomValid ? mGeomMax : mWindow.IsMaximized();
			out += gmax ? "maximized=1\n" : "maximized=0\n";
			NkFile::WriteAllText(p, out);
		}

		bool NkEditorShell::LoadWindowGeom(const char *path) noexcept {
			if (!path || !*path || !NkFile::Exists(path))
				return false;
			const NkString txt = NkFile::ReadAllText(NkPath(path));
			NkString line;
			auto apply = [&](const char *s) {
				if (StartsWith(s, "win=")) { // taille seule (pas la position)
					int32 x = 0, y = 0, w = 0, h = 0;
					if (std::sscanf(s + 4, "%d|%d|%d|%d", &x, &y, &w, &h) == 4 && w > 200 && h > 150 && w < 20000 &&
						h < 20000) {
						if (mWindow.IsMaximized())
							mWindow.Restore();
						mWindow.SetSize(static_cast<uint32>(w), static_cast<uint32>(h));
					}
				} else if (StartsWith(s, "maximized=") && s[10] == '1') {
					if (!mWindow.IsMaximized()) // toggle -> guarder
						mWindow.Maximize();
				}
			};
			for (const char *c = txt.CStr();; ++c) {
				if (*c == '\n' || *c == '\r' || *c == '\0') {
					if (!line.Empty()) {
						apply(line.CStr());
						line.Clear();
					}
					if (*c == '\0')
						break;
				} else
					line += *c;
			}
			return true;
		}

		void NkEditorShell::SaveUiState(const char *path) noexcept {
			if (!path || !*path)
				return;
			NkPath p(path);
			NkDirectory::CreateRecursive(p.GetParent()); // cree <ws>/.nkcode/ si besoin
			NkString out;
			// Géométrie CACHÉE en vol (la fenêtre peut être détruite après Run()).
			// On sauve la TAILLE fenêtrée (restaurée telle quelle sur le même moniteur)
			// + l'état maximisé. La POSITION n'est PAS restaurée (éviterait un changement
			// de moniteur/DPI qui désynchronise l'échelle).
			const bool gmax = mGeomValid ? mGeomMax : mWindow.IsMaximized();
			if (!gmax)
				out += NkPrintf("win=%d|%d|%d|%d\n", mGeomX, mGeomY, mGeomW, mGeomH);
			out += gmax ? "maximized=1\n" : "maximized=0\n";
			for (int32 i = 0; i < mNumPanels; ++i)
				if (mPanels[i]->IsOpen()) {
					out += "panel=";
					out += mPanels[i]->Title();
					out += "\n";
				}
			// ── Disposition du dock : arbre CENTRAL sérialisé en DFS (indices remappés
			//    0..n-1, racine = 0) + position des fenêtres flottantes des panneaux.
			//    Les hôtes de dock flottants (arbres secondaires) ne sont pas sérialisés :
			//    leurs fenêtres redeviennent flottantes simples à la restauration. ──
			if (mUI.dockRoot >= 0 && mUI.dockRoot < static_cast<int32>(mUI.dockNodes.Size())) {
				int32 map[256], order[256], n = 0;
				for (int32 i = 0; i < 256; ++i)
					map[i] = -1;
				int32 stack[256], top = 0;
				stack[top++] = mUI.dockRoot;
				while (top > 0 && n < 256) {
					const int32 cur = stack[--top];
					if (cur < 0 || cur >= static_cast<int32>(mUI.dockNodes.Size()) || cur >= 256 || map[cur] >= 0)
						continue;
					map[cur] = n;
					order[n++] = cur;
					const NkGuiDockNode &d = mUI.dockNodes[cur];
					if (d.kind == 1 && top < 254) {
						stack[top++] = d.child1;
						stack[top++] = d.child0;
					}
				}
				out += "dockroot=0\n";
				char buf[192];
				for (int32 k = 0; k < n; ++k) {
					const NkGuiDockNode &d = mUI.dockNodes[order[k]];
					const int32 c0 = (d.kind == 1 && d.child0 >= 0 && d.child0 < 256) ? map[d.child0] : -1;
					const int32 c1 = (d.kind == 1 && d.child1 >= 0 && d.child1 < 256) ? map[d.child1] : -1;
					std::snprintf(buf, sizeof(buf), "node=%d|%d|%d|%.4f|%d|%d|%d\n", k, static_cast<int32>(d.kind),
								  d.vertical ? 1 : 0, static_cast<double>(d.ratio), c0, c1, d.activeTab);
					out += buf;
					if (d.kind == 2)
						for (int32 w = 0; w < d.winCount; ++w)
							for (uint32 m2 = 0; m2 < mUI.windowMeta.Size(); ++m2)
								if (mUI.windowMeta[m2].id == d.windows[w]) {
									if (mUI.windowMeta[m2].title[0]) {
										std::snprintf(buf, sizeof(buf), "nwin=%d|%s\n", k, mUI.windowMeta[m2].title);
										out += buf;
									}
									break;
								}
				}
				for (int32 i = 0; i < mNumPanels; ++i) {
					const NkGuiId wid = mUI.GetId(mPanels[i]->Title());
					for (uint32 m2 = 0; m2 < mUI.windowMeta.Size(); ++m2) {
						const NkGuiWindowMeta &wm = mUI.windowMeta[m2];
						if (wm.id != wid)
							continue;
						if (wm.dockNode < 0 && wm.hostRoot < 0 && wm.init) {
							std::snprintf(buf, sizeof(buf), "float=%.1f|%.1f|%.1f|%.1f|%s\n",
										  static_cast<double>(wm.rect.x), static_cast<double>(wm.rect.y),
										  static_cast<double>(wm.rect.w), static_cast<double>(wm.rect.h),
										  mPanels[i]->Title());
							out += buf;
						}
						break;
					}
				}
			}
			NkFile::WriteAllText(p, out);
		}

		// ── Panneaux (le docking est gere DANS Begin) ────────────────────────────
		void NkEditorShell::DrawPanels(NkEditorFrameContext &ec) noexcept {
			const float32 menuH = mUI.ItemHeight();
			for (int32 i = 0; i < mNumPanels; ++i) {
				NkEditorPanel *p = mPanels[i];
				if (!p->IsOpen())
					continue;
				if (mDockBootstrap && !p->Dockable()) { // 1er placement des flottants
					SetNextWindowPos(mUI, 60.f + i * 28.f, menuH + 40.f + i * 28.f);
					SetNextWindowSize(mUI, 360.f, 280.f);
				}
				// La grande barre externe se debranche par SetDockScrollbarVisible
				// (molette conservee — les panneaux de NkUIDesign portent leurs
				// propres ascenseurs par section).
				if (Begin(mUI, p->Title(), p->OpenPtr(),
						  mDockScrollbars ? nkgui::NkGuiWindowFlags::None
										  : nkgui::NkGuiWindowFlags::NoScrollbar)) {
					// Une fenêtre FLOTTANTE recouvre la souris et ce n'est pas la nôtre ->
					// souris neutralisée pendant OnUI : le code custom des panneaux (éditeur,
					// arbres) lit l'input en direct et recevrait sinon clics/molette À TRAVERS
					// la fenêtre du dessus — et lui volerait son drag de barre de titre.
					const bool shielded =
						mUI.hoveredWindowId != NKGUI_ID_NONE && mUI.hoveredWindowId != mUI.curWindowId;
					nkgui::NkGuiInput saved;
					if (shielded) {
						saved = mUI.input;
						mUI.input.mousePos = {-100000.f, -100000.f};
						for (int32 b = 0; b < 3; ++b) {
							mUI.input.mouseClicked[b] = false;
							mUI.input.mouseDown[b] = false;
							mUI.input.mouseDoubleClicked[b] = false;
						}
						mUI.input.wheel = mUI.input.wheelH = 0.f;
					}
					p->OnUI(ec);
					if (shielded)
						mUI.input = saved;
					EndWindow(mUI);
				}
			}
		}

		// ── Bootstrap docking : disposition par defaut (centre puis cotes) ───────
		void NkEditorShell::BootstrapDocking() noexcept {
			if (!mDockBootstrap)
				return;
			// Centre d'abord, puis les côtés. Les panneaux d'un MÊME côté sont
			// regroupés en ONGLETS (ex. Terminal + Sortie partagent la barre du bas).
			for (int32 pass = 0; pass < 2; ++pass) {
				const bool centerPass = (pass == 0);
				const char *sideFirst[8] = {}; // 1er panneau ancré par zone (les suivants = onglets)
				for (int32 i = 0; i < mNumPanels; ++i) {
					NkEditorPanel *p = mPanels[i];
					if (!p->IsOpen() || !p->Dockable())
						continue;
					const bool isCenter = (p->DefaultSide() == NkEditorDockSide::NK_CENTER);
					if (isCenter != centerPass)
						continue;
					const int32 zone = SideToZone(p->DefaultSide());
					if (zone >= 0 && zone < 8 && sideFirst[zone])
						DockBuilderDockTab(mUI, p->Title(), sideFirst[zone]);
					else {
						DockBuilderDock(mUI, p->Title(), zone);
						if (zone >= 0 && zone < 8)
							sideFirst[zone] = p->Title();
					}
				}
			}
			mDockBootstrap = false;
		}

		// ── Palette de commandes (overlay, Ctrl+P) ───────────────────────────────
		void NkEditorShell::DrawCommandPalette(NkEditorFrameContext &) noexcept {
			if (!mPaletteOpen || !mFontOk)
				return;
			const float32 W = static_cast<float32>(mUI.viewW);
			const float32 H = static_cast<float32>(mUI.viewH);

			// ── Surface flottante DECLAREE au routeur d'occlusion ────────────────
			// Meme patron que les trois autres surfaces du kit : menu contextuel
			// (NkEditorContextMenu.h, couche 50), modale (NkEditorModal.h, 100),
			// selecteur de fichiers (NkFilePicker.h, 100). La palette etait la
			// SEULE a ne pas se declarer.
			// Sans ces deux lignes, le voile plein ecran dessine juste dessous est
			// purement VISUEL : un widget de couche 0 (panneau ancre) reste
			// survolable et cliquable en dessous, parce que ItemHoverable consulte
			// PointReachable et que rien ne lui avait declare cette surface.
			// Mesure du 2026-08-17 (Nogee, --occlusion-test), temoin a l'appui :
			// panneau ancre -> ItemHoverable = 1 palette FERMEE **et** 1 palette
			// OUVERTE, donc le clic traversait.
			mUI.PushOcclusion({0.f, 0.f, W, H}, 50);
			NkGuiContext::NkInputLayerScope _paletteLayer(mUI, 50);

			const float32 pw = 480.f, rowH = mUI.ItemHeight() + 4.f, headH = mUI.ItemHeight() + 12.f;
			const int32 count = mNumCommands;
			const float32 ph = headH + 6.f + count * rowH + 8.f;
			const float32 px = (W - pw) * 0.5f, py = H * 0.16f;
			const float32 asc = mFont.Ascent(), lh = mFont.LineHeight();
			auto &dl = mUI.dlOverlay;

			dl.AddRectFilled({0.f, 0.f, W, H}, kBackdrop);
			dl.AddRectFilled({px, py, pw, ph}, kPaletteBg, 8.f);
			dl.AddRect({px, py, pw, ph}, kPaletteBorder, 1.5f);
			dl.AddText(mFont.Face(), mFont.TexId(), {px + 14.f, py + 9.f + asc}, "Palette de commandes", kTextPrimary);

			const float32 listTop = py + headH + 2.f;
			for (int32 i = 0; i < count; ++i) {
				const NkRect r = {px + 6.f, listTop + i * rowH, pw - 12.f, rowH - 2.f};
				const bool hov = NkGuiRectContains(r, mUI.input.mousePos);
				if (hov)
					mPaletteSel = i;
				const bool sel = (i == mPaletteSel);
				if (sel)
					dl.AddRectFilled(r, kPaletteSel, 5.f);

				const float32 ty = r.y + (r.h - lh) * 0.5f + asc;
				dl.AddText(mFont.Face(), mFont.TexId(), {r.x + 12.f, ty}, mCommands[i].name,
						   sel ? kTextPrimary : kTextSecondary);
				if (mCommands[i].shortcut[0]) {
					const float32 sw = mFont.MeasureWidth(mCommands[i].shortcut);
					dl.AddText(mFont.Face(), mFont.TexId(), {r.x + r.w - 12.f - sw, ty}, mCommands[i].shortcut,
							   kTextTertiary);
				}
				if (hov && mUI.input.mouseClicked[0]) {
					ExecuteCommand(i);
					mPaletteOpen = false;
				}
			}
		}

		// ── Polices : (re)charge mFont (interface) + mCodeFont (code) depuis les
		//    reglages, avec replis surs, puis re-upload des atlas au backend. ──
		// Recharge les DEUX polices (interface + code). Appelé au démarrage et quand la
		// police d'interface change. Le ZOOM, lui, n'appelle QUE LoadCodeFont() -> pas de
		// reconstruction inutile de l'atlas d'interface à chaque cran.
		void NkEditorShell::LoadFontsFromPrefs() noexcept {
			LoadUiFont();
			LoadCodeFont();
			LoadTermFont();
		}

		void NkEditorShell::LoadUiFont() noexcept {
			// Police chargee a uiSize x DPI : le LAYOUT est mis a l'echelle (ctx.S) mais
			// la police etait a une taille ABSOLUE -> texte trop petit sur ecran scale.
			// On la met a la meme echelle pour des proportions correctes (lisible).
			const float32 dpi = mUI.S(1.f) > 0.5f ? mUI.S(1.f) : 1.f;
			const float32 uiPx = mFontPrefs.uiSize * dpi;
			mFontOk = NkResolveFont(mFont, mFontPrefs.uiFont, uiPx);
			if (!mFontOk)
				mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::Inter, uiPx);
			if (!mFontOk)
				mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::Karla, 16.f * dpi);
			if (!mFontOk)
				mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 13.f * dpi);
			mUI.font = &mFont;
			if (mFontOk && mRenderer)
				mRenderer->UploadFontGray8(mFont.TexId(), mFont.pixels, mFont.atlasW, mFont.atlasH);
		}

		void NkEditorShell::LoadCodeFont() noexcept {
			const float32 dpi = mUI.S(1.f) > 0.5f ? mUI.S(1.f) : 1.f;
			// mCodeFont = REPLI a la taille GLOBALE (l'editeur, lui, rend via le cache par taille,
			// cf. CodeFontForSize). Sert de police par defaut avant que l'editeur ne pilote.
			const float32 logical = mFontPrefs.codeSize;
			const float32 codePx = logical * dpi;
			mCodeLoadedSize = logical;
			mCodeFont.texId = mFont.TexId() + 1u; // atlas distinct (anti-collision backend)
			// Police MONOSPACE : AUCUN repli externe (broad/CJK/emoji = plusieurs milliers de
			// glyphes). L'atlas reste petit -> reconstruction rapide (l'interface garde les
			// replis complets pour l'i18n). La police embarquee couvre deja Latin/accents/box-drawing.
			bool codeOk = NkResolveFont(mCodeFont, mFontPrefs.codeFont, codePx, /*extFallback=*/false);
			if (!codeOk)
				codeOk = mCodeFont.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, codePx, /*extFallback=*/false);
			if (!codeOk)
				codeOk = mCodeFont.LoadEmbedded(NkEmbeddedFontId::Cousine, 15.f * dpi, /*extFallback=*/false);
			if (codeOk) {
				if (mRenderer)
					mRenderer->UploadFontGray8(mCodeFont.TexId(), mCodeFont.pixels, mCodeFont.atlasW, mCodeFont.atlasH);
				mUI.codeFont = &mCodeFont;
			} else
				mUI.codeFont = &mFont;
		}

		// Police du TERMINAL : atlas SEPARE a la taille GLOBALE (mFontPrefs.codeSize), JAMAIS
		// pilote par le zoom par-onglet -> le terminal ne change pas quand on zoome un fichier.
		// texId +2 (editeur = +1) pour un atlas backend distinct. Rechargee uniquement au boot
		// et sur changement de prefs (rare), donc rebuild inconditionnel = OK.
		void NkEditorShell::LoadTermFont() noexcept {
			const float32 dpi = mUI.S(1.f) > 0.5f ? mUI.S(1.f) : 1.f;
			// Taille du terminal = son propre zoom (mTermTargetSize) si demande, sinon globale.
			const float32 logical = mTermTargetSize > 0.f ? mTermTargetSize : mFontPrefs.codeSize;
			const float32 codePx = logical * dpi;
			mTermLoadedSize = logical;
			mTermFont.texId = mFont.TexId() + 2u;
			bool ok = NkResolveFont(mTermFont, mFontPrefs.codeFont, codePx, /*extFallback=*/false);
			if (!ok)
				ok = mTermFont.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, codePx, /*extFallback=*/false);
			if (!ok)
				ok = mTermFont.LoadEmbedded(NkEmbeddedFontId::Cousine, 15.f * dpi, /*extFallback=*/false);
			if (ok && mRenderer)
				mRenderer->UploadFontGray8(mTermFont.TexId(), mTermFont.pixels, mTermFont.atlasW, mTermFont.atlasH);
		}

		nkgui::NkGuiFont *NkEditorShell::TermCodeFont() noexcept {
			return mTermFont.Valid() ? &mTermFont : mUI.codeFont;
		}

		float32 NkEditorShell::TermSize() const noexcept {
			return mTermTargetSize > 0.f ? mTermTargetSize : mFontPrefs.codeSize;
		}

		// Demande la taille de l'atlas TERMINAL (0 = globale). Rebuild debounce ; comme le code,
		// ne (re)arme que quand la cible change (l'app appelle chaque frame).
		void NkEditorShell::RequestTermSize(float32 logicalPx) noexcept {
			if (logicalPx > 0.f) {
				if (logicalPx < 8.f)
					logicalPx = 8.f;
				if (logicalPx > 40.f)
					logicalPx = 40.f;
			}
			if (logicalPx == mTermTargetSize)
				return;
			mTermTargetSize = logicalPx;
			const float32 eff = logicalPx > 0.f ? logicalPx : mFontPrefs.codeSize;
			mTermReloadCountdown = (eff == mTermLoadedSize) ? -1.f : kCodeReloadDebounce;
		}

		// ── Zoom editeur : ajuste la taille de la police du code puis reconstruit ──
		float32 NkEditorShell::CodeFontSize() const noexcept {
			return mFontPrefs.codeSize;
		}

		float32 NkEditorShell::ActiveCodeSize() const noexcept {
			return mCodeActiveLogical > 0.f ? mCodeActiveLogical : mFontPrefs.codeSize;
		}

		static inline int32 NkCodePxOf(float32 logical, float32 global) {
			int32 px = static_cast<int32>((logical > 0.f ? logical : global) + 0.5f);
			return px < 8 ? 8 : (px > 40 ? 40 : px);
		}

		// Arme la rasterisation d'une taille si elle n'est PAS deja en cache. immediate
		// (changement d'onglet) -> des la frame suivante ; sinon debounce (zoom molette).
		void NkEditorShell::EnsureCodeSize(float32 logicalPx, bool immediate) noexcept {
			mCodeActiveLogical = logicalPx;
			const int32 px = NkCodePxOf(logicalPx, mFontPrefs.codeSize);
			for (int32 i = 0; i < kCodeCacheN; ++i)
				if (mCodeSlots[i].font && mCodeSlots[i].px == px)
					return; // deja pret -> rien
			if (px == mCodePendingPx && mCodeReloadCountdown >= 0.f)
				return; // deja arme pour ce px
			mCodePendingPx = px;
			mCodeReloadCountdown = immediate ? 0.f : kCodeReloadDebounce;
		}

		// Police a utiliser MAINTENANT pour cette taille : exacte en cache si dispo, sinon la plus
		// proche deja rasterisee (ou le repli global) en attendant le build. NON bloquant.
		nkgui::NkGuiFont *NkEditorShell::CodeFontForSize(float32 logicalPx) noexcept {
			const int32 px = NkCodePxOf(logicalPx, mFontPrefs.codeSize);
			nkgui::NkGuiFont *best = (mCodeFont.Valid() ? &mCodeFont : &mFont);
			int32 bestDiff = 0x7FFFFFFF;
			for (int32 i = 0; i < kCodeCacheN; ++i) {
				if (!mCodeSlots[i].font)
					continue;
				if (mCodeSlots[i].px == px) {
					mCodeSlots[i].lru = ++mCodeClock;
					return mCodeSlots[i].font;
				}
				const int32 d = px - mCodeSlots[i].px, ad = d < 0 ? -d : d;
				if (ad < bestDiff) {
					bestDiff = ad;
					best = mCodeSlots[i].font;
				}
			}
			return best;
		}

		// Rasterise `px` dans le cache (slot vide sinon LRU). Appele HORS frame (debounce expire).
		void NkEditorShell::BuildCodeSlot(int32 px) noexcept {
			if (px < 8)
				px = 8;
			if (px > 40)
				px = 40;
			int32 idx = -1;
			for (int32 i = 0; i < kCodeCacheN; ++i) {
				if (mCodeSlots[i].font && mCodeSlots[i].px == px) {
					mCodeSlots[i].lru = ++mCodeClock;
					return;
				} // course : deja fait
				if (!mCodeSlots[i].font && idx < 0)
					idx = i;
			}
			if (idx < 0) {
				uint32 lru = 0xFFFFFFFFu;
				for (int32 i = 0; i < kCodeCacheN; ++i)
					if (mCodeSlots[i].lru < lru) {
						lru = mCodeSlots[i].lru;
						idx = i;
					}
			}
			CodeSlot &s = mCodeSlots[idx];
			if (!s.font)
				s.font = nkentseu::memory::NkGetDefaultAllocator().New<nkgui::NkGuiFont>();
			if (!s.font)
				return;
			s.font->texId =
				mFont.TexId() + 8u + static_cast<uint32>(idx); // texIds cache = +8..+15 (distincts de +1/+2)
			const float32 dpi = mUI.S(1.f) > 0.5f ? mUI.S(1.f) : 1.f;
			const float32 codePx = static_cast<float32>(px) * dpi;
			bool ok = NkResolveFont(*s.font, mFontPrefs.codeFont, codePx, /*extFallback=*/false);
			if (!ok)
				ok = s.font->LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, codePx, /*extFallback=*/false);
			if (!ok)
				ok = s.font->LoadEmbedded(NkEmbeddedFontId::Cousine, 15.f * dpi, /*extFallback=*/false);
			if (!ok) {
				s.px = 0;
				return;
			}
			if (mRenderer)
				mRenderer->UploadFontGray8(s.font->TexId(), s.font->pixels, s.font->atlasW, s.font->atlasH);
			s.px = px;
			s.lru = ++mCodeClock;
		}

		void NkEditorShell::NudgeCodeFontSize(float32 delta) noexcept {
			if (mZoomFn) {
				mZoomFn(mZoomUser, delta, false);
				return;
			} // zoom PAR ONGLET (app)
			float32 s = mFontPrefs.codeSize + delta;
			if (s < 8.f)
				s = 8.f;
			if (s > 40.f)
				s = 40.f;
			if (s == mFontPrefs.codeSize)
				return;
			mFontPrefs.codeSize = s;
			// DEBOUNCE : on ne reconstruit PAS l'atlas a chaque cran (couteux). On (re)arme un
			// compte a rebours ; l'atlas code est reconstruit ~120 ms apres le DERNIER cran ->
			// molette fluide, un seul rebuild a la fin. (Reconstruit hors frame, cf. RenderFrame.)
			mCodeReloadCountdown = kCodeReloadDebounce;
			NkSaveFontPrefs(mFontPrefs); // persiste la taille
		}

		// Réinitialise la police du code à la taille par défaut (Ctrl+0), reload différé + persiste.
		void NkEditorShell::ResetCodeFontSize() noexcept {
			if (mZoomFn) {
				mZoomFn(mZoomUser, 0.f, true);
				return;
			} // reset PAR ONGLET (app)
			if (mFontPrefs.codeSize == kDefaultCodeFontSize)
				return;
			mFontPrefs.codeSize = kDefaultCodeFontSize;
			mCodeReloadCountdown = kCodeReloadDebounce;
			NkSaveFontPrefs(mFontPrefs);
		}

		// ── Fenetre Preferences (overlay, menu dedie) : categories a gauche
		//    (Polices, Theme, ... extensible), contenu a droite. ──
		void NkEditorShell::DrawPreferences(NkEditorFrameContext &) noexcept {
			if (!mShowPrefs || !mFontOk)
				return;
			const float32 W = static_cast<float32>(mUI.viewW), H = static_cast<float32>(mUI.viewH);
			const float32 asc = mFont.Ascent(), lh = mFont.LineHeight();
			auto &dl = mUI.dlOverlay;
			const NkVec2 mp = mUI.input.mousePos;
			const bool click = mUI.input.mouseClicked[0];
			auto hit = [&](const NkRect &r) { return NkGuiRectContains(r, mp); };

			const float32 pw = 620.f, ph = 505.f, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
			dl.AddRectFilled({0.f, 0.f, W, H}, kBackdrop);
			// Clic hors fenetre -> ferme (sauf la frame d'ouverture : le clic du menu
			// est lui-meme hors du popup centre, il fermerait aussitot).
			if (mPrefsJustOpened)
				mPrefsJustOpened = false;
			else if (click && !NkGuiRectContains({px, py, pw, ph}, mp)) {
				mShowPrefs = false;
			}
			dl.AddRectFilled({px, py, pw, ph}, kPaletteBg, 8.f);
			dl.AddRect({px, py, pw, ph}, kPaletteBorder, 1.5f);
			dl.AddText(mFont.Face(), mFont.TexId(), {px + 18.f, py + 14.f + asc}, "Preferences", kTextPrimary);

			auto text = [&](float32 x, float32 y, const char *s, const NkColor &c) {
				dl.AddText(mFont.Face(), mFont.TexId(), {x, y + asc}, s, c);
			};
			auto btn = [&](const NkRect &r, const char *s, bool enabled) -> bool {
				const bool hov = enabled && hit(r);
				dl.AddRectFilled(r, hov ? kPaletteSel : NkColor{40, 46, 54, 255}, 5.f);
				dl.AddRect(r, NkColor{60, 66, 74, 255}, 1.f);
				const float32 tw = mFont.MeasureWidth(s);
				dl.AddText(mFont.Face(), mFont.TexId(), {r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f + asc}, s,
						   enabled ? kTextPrimary : kTextTertiary);
				return hov && click;
			};
			auto cycle = [&](NkString &name, const char *const *list, int32 n, int32 dir) {
				int32 idx = 0;
				for (int32 i = 0; i < n; ++i)
					if (name == list[i]) {
						idx = i;
						break;
					}
				idx = (idx + dir + n) % n;
				name = list[idx];
			};

			// ── Sidebar des categories ──
			const float32 sideW = 150.f, cy0 = py + 54.f, catH = 34.f;
			dl.AddRectFilled({px + 1.f, py + 44.f, sideW, ph - 45.f}, NkColor{1, 4, 9, 255});
			const char *cats[] = {"Polices", "Theme", "Langages"};
			for (int32 i = 0; i < 3; ++i) {
				const NkRect r = {px + 6.f, cy0 + i * catH, sideW - 12.f, catH - 4.f};
				const bool active = (mPrefsTab == i);
				if (active)
					dl.AddRectFilled(r, kPaletteSel, 5.f);
				else if (hit(r))
					dl.AddRectFilled(r, NkColor{33, 39, 48, 255}, 5.f);
				text(r.x + 12.f, r.y + (r.h - lh) * 0.5f, cats[i], active ? kTextPrimary : kTextSecondary);
				if (hit(r) && click)
					mPrefsTab = i;
			}

			const float32 cx = px + sideW + 24.f; // colonne contenu
			float32 y = py + 60.f;

			if (mPrefsTab == 0) {
				// ── Categorie Polices ──
				int32 nUi = 0, nCode = 0;
				const char *const *uiList = NkUiFontNames(&nUi);
				const char *const *codeList = NkCodeFontNames(&nCode);
				auto fontRow = [&](const char *lab, NkString &name, const char *const *list, int32 n, float32 &sz) {
					text(cx, y, lab, kTextSecondary);
					y += 24.f;
					if (btn({cx, y, 28.f, 26.f}, "<", true))
						cycle(name, list, n, -1);
					const NkRect nameBox = {cx + 32.f, y, 160.f, 26.f};
					dl.AddRectFilled(nameBox, NkColor{22, 27, 34, 255}, 4.f);
					dl.AddRect(nameBox, NkColor{48, 54, 61, 255}, 1.f);
					text(nameBox.x + 10.f, nameBox.y + (26.f - lh) * 0.5f, name.CStr(), kTextPrimary);
					if (btn({cx + 196.f, y, 28.f, 26.f}, ">", true))
						cycle(name, list, n, 1);
					// Taille
					text(cx + 246.f, y + (26.f - lh) * 0.5f, "Taille", kTextSecondary);
					if (btn({cx + 310.f, y, 26.f, 26.f}, "-", true)) {
						sz -= 1.f;
						if (sz < 8.f)
							sz = 8.f;
					}
					char sb[8];
					std::snprintf(sb, sizeof(sb), "%d", static_cast<int>(sz + 0.5f));
					const NkRect szBox = {cx + 338.f, y, 36.f, 26.f};
					dl.AddRectFilled(szBox, NkColor{22, 27, 34, 255}, 4.f);
					const float32 sw = mFont.MeasureWidth(sb);
					text(szBox.x + (szBox.w - sw) * 0.5f, szBox.y + (26.f - lh) * 0.5f, sb, kTextPrimary);
					if (btn({cx + 376.f, y, 26.f, 26.f}, "+", true)) {
						sz += 1.f;
						if (sz > 40.f)
							sz = 40.f;
					}
					y += 44.f;
				};
				fontRow("Police de l'interface", mFontPrefs.uiFont, uiList, nUi, mFontPrefs.uiSize);
				fontRow("Police du code et du terminal", mFontPrefs.codeFont, codeList, nCode, mFontPrefs.codeSize);
				y += 6.f;
				text(cx, y, "Astuce : Consolas / Segoe UI / Courier New = polices systeme", kTextTertiary);
				y += 18.f;
				text(cx, y, "(chargees depuis Windows si presentes).", kTextTertiary);
				y += 28.f;
				if (btn({cx, y, 120.f, 30.f}, "Appliquer", true)) {
					NkSaveFontPrefs(mFontPrefs);
					mFontReloadPending = true;
				}
				if (btn({cx + 132.f, y, 150.f, 30.f}, "Reinitialiser", true)) {
					mFontPrefs = NkFontPrefs{}; // defaut Inter + DejaVu
					NkSaveFontPrefs(mFontPrefs);
					mFontReloadPending = true;
				}
			} else if (mPrefsTab == 1) {
				// ── Categorie Theme : couleur de l'interface, NOMMEE par element ──
				struct TE {
						const char *n;
						NkColor *c;
				};

				TE ents[] = {
					{"Fond principal (editeur)", &mUI.theme.bgPrimary},
					{"Panneaux", &mUI.theme.panel},
					{"En-tete / barre de menus", &mUI.theme.header},
					{"Bordures", &mUI.theme.border},
					{"Texte", &mUI.theme.text},
					{"Texte desactive", &mUI.theme.textDisabled},
					{"Accent (focus, barres)", &mUI.theme.accent},
					{"Element selectionne", &mUI.theme.selection},
					{"Bouton", &mUI.theme.button},
					{"Bouton (survol)", &mUI.theme.buttonHover},
					{"Barre d'onglets", &mUI.theme.tabBar},
					{"Onglet actif", &mUI.theme.tabActive},
					{"Onglet inactif", &mUI.theme.tab},
					{"Onglet (survol)", &mUI.theme.tabHover},
					{"Pistes de defilement", &mUI.theme.track},
					{"Bouton (actif)", &mUI.theme.buttonActive},
				};
				const int32 ne = 16;
				if (mThemeSel < 0 || mThemeSel >= ne)
					mThemeSel = 6;
				text(cx, y, "Theme de l'interface - couleur par element :", kTextSecondary);
				y += 24.f;

				// Liste 2 colonnes : pastille + nom, cliquable (selection).
				const float32 colW = 196.f, rowH = 22.f;
				const float32 gridY = y;
				for (int32 i = 0; i < ne; ++i) {
					const int32 col = i % 2, row = i / 2;
					const NkRect rr = {cx + col * colW, gridY + row * rowH, colW - 8.f, rowH - 2.f};
					const bool sel = (mThemeSel == i);
					if (sel)
						dl.AddRectFilled(rr, kPaletteSel, 4.f);
					else if (hit(rr))
						dl.AddRectFilled(rr, NkColor{33, 39, 48, 255}, 4.f);
					const NkRect sw = {rr.x + 4.f, rr.y + 3.f, 15.f, 15.f};
					dl.AddRectFilled(sw, *ents[i].c, 3.f);
					dl.AddRect(sw, NkColor{60, 66, 74, 255}, 1.f);
					text(sw.x + 22.f, rr.y + (rowH - 2.f - lh) * 0.5f, ents[i].n, sel ? kTextPrimary : kTextSecondary);
					if (hit(rr) && click)
						mThemeSel = i;
				}
				y = gridY + ((ne + 1) / 2) * rowH + 14.f;

				// Palette : applique une couleur a l'element selectionne.
				text(cx, y, ents[mThemeSel].n, kTextPrimary);
				y += 22.f;
				static const NkColor pal[] = {
					{13, 17, 23, 255},	  {22, 27, 34, 255},	{33, 38, 45, 255},	  {48, 54, 61, 255},
					{110, 118, 129, 255}, {201, 209, 217, 255}, {240, 246, 252, 255}, {255, 255, 255, 255},
					{31, 111, 235, 255},  {56, 139, 253, 255},	{163, 113, 247, 255}, {188, 140, 255, 255},
					{63, 185, 80, 255},	  {86, 211, 100, 255},	{219, 109, 40, 255},  {248, 81, 73, 255},
				};
				for (int32 j = 0; j < 16; ++j) {
					const NkRect r = {cx + (j % 8) * 30.f, y + (j / 8) * 30.f, 26.f, 26.f};
					dl.AddRectFilled(r, pal[j], 5.f);
					dl.AddRect(r, NkColor{60, 66, 74, 255}, 1.f);
					if (hit(r) && click) {
						const uint8 a = ents[mThemeSel].c->a;
						*ents[mThemeSel].c = pal[j];
						ents[mThemeSel].c->a = a;
					}
				}
				y += 70.f;
				// Enregistrer / Charger (fichier) + Reinitialiser (theme par defaut de l'app).
				if (btn({cx, y, 130.f, 30.f}, "Enregistrer", true))
					NkSaveTheme(mUI.theme);
				if (btn({cx + 142.f, y, 110.f, 30.f}, "Charger", true))
					NkLoadTheme(mUI.theme);
				if (btn({cx + 264.f, y, 150.f, 30.f}, "Reinitialiser", true))
					mUI.theme = mDefaultTheme;
				y += 38.f;
				text(cx, y, "Enregistre/charge le theme dans ~/.nkcode_theme.cfg.", kTextTertiary);
			} else {
				// ── Categorie Langages : couleurs de coloration syntaxique ──
				struct SE {
						const char *n;
						NkColor *c;
				};

				SE ents[] = {
					{"Texte normal", &mUI.syntax.text},
					{"Mot-cle", &mUI.syntax.keyword},
					{"Type", &mUI.syntax.type},
					{"Chaine de caracteres", &mUI.syntax.string},
					{"Commentaire", &mUI.syntax.comment},
					{"Nombre", &mUI.syntax.number},
					{"Preprocesseur / macro", &mUI.syntax.preproc},
					{"Titre (Markdown)", &mUI.syntax.heading},
					{"Code (Markdown)", &mUI.syntax.mdcode},
				};
				const int32 ne = 9;
				if (mSynSel < 0 || mSynSel >= ne)
					mSynSel = 1;
				text(cx, y, "Coloration syntaxique (C/C++, Python, NKSL, Markdown) :", kTextSecondary);
				y += 24.f;
				const float32 rowH = 25.f, gridY = y;
				// Liste : chaque token affiche DANS sa propre couleur (apercu direct).
				for (int32 i = 0; i < ne; ++i) {
					const NkRect rr = {cx, gridY + i * rowH, 264.f, rowH - 3.f};
					const bool sel = (mSynSel == i);
					if (sel)
						dl.AddRectFilled(rr, kPaletteSel, 4.f);
					else if (hit(rr))
						dl.AddRectFilled(rr, NkColor{33, 39, 48, 255}, 4.f);
					const NkRect sw = {rr.x + 4.f, rr.y + 3.f, 15.f, 15.f};
					dl.AddRectFilled(sw, *ents[i].c, 3.f);
					dl.AddRect(sw, NkColor{60, 66, 74, 255}, 1.f);
					text(sw.x + 24.f, rr.y + (rowH - 3.f - lh) * 0.5f, ents[i].n, *ents[i].c);
					if (hit(rr) && click)
						mSynSel = i;
				}
				// Palette a droite : applique au token selectionne.
				const float32 palX = cx + 286.f;
				text(palX, gridY - 24.f + 4.f, ents[mSynSel].n, kTextPrimary);
				static const NkColor sp[] = {
					{212, 212, 212, 255}, {86, 156, 214, 255},	{78, 201, 176, 255},  {206, 145, 120, 255},
					{106, 153, 85, 255},  {181, 206, 168, 255}, {197, 134, 192, 255}, {220, 220, 170, 255},
					{156, 220, 254, 255}, {244, 71, 71, 255},	{215, 186, 125, 255}, {96, 139, 78, 255},
					{255, 255, 255, 255}, {110, 118, 129, 255}, {86, 211, 100, 255},  {255, 123, 114, 255},
				};
				for (int32 j = 0; j < 16; ++j) {
					const NkRect r = {palX + (j % 4) * 30.f, gridY + (j / 4) * 30.f, 26.f, 26.f};
					dl.AddRectFilled(r, sp[j], 5.f);
					dl.AddRect(r, NkColor{60, 66, 74, 255}, 1.f);
					if (hit(r) && click) {
						const uint8 a = ents[mSynSel].c->a;
						*ents[mSynSel].c = sp[j];
						ents[mSynSel].c->a = a;
					}
				}
				float32 by = gridY + ne * rowH + 12.f;
				if (btn({cx, by, 130.f, 30.f}, "Enregistrer", true))
					NkSaveSyntax(mUI.syntax);
				if (btn({cx + 142.f, by, 110.f, 30.f}, "Charger", true))
					NkLoadSyntax(mUI.syntax);
				if (btn({cx + 264.f, by, 150.f, 30.f}, "Reinitialiser", true))
					mUI.syntax = mDefaultSyntax;
			}

			// ── Bouton Fermer (bas-droite) ──
			if (btn({px + pw - 110.f, py + ph - 40.f, 96.f, 30.f}, "Fermer", true))
				mShowPrefs = false;
		}

	} // namespace editorkit
} // namespace nkentseu
