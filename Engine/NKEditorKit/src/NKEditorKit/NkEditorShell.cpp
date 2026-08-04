// =============================================================================
// NkEditorShell.cpp — implementation de la coquille d'editeur (sur NKGui).
//   events -> BeginFrame -> menubar -> DockSpace -> panneaux -> palette -> rendu.
// =============================================================================
#include "NKEditorKit/NkEditorShell.h"
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
			NkGuiTheme &t = mUI.theme;
			t.bgPrimary = {13, 17, 23, 255}; // editeur #0D1117
			t.panel = {1, 4, 9, 255};		 // sidebar #010409 (plus sombre)
			t.header = {25, 29, 35, 255};	 // titres/menus #191D23
			t.button = {25, 29, 35, 255};
			t.buttonHover = {33, 39, 48, 255};	   // hover liste (#191D23 + un poil)
			t.buttonActive = {31, 111, 235, 255};  // selection active #1F6FEB
			t.border = {33, 39, 48, 255};		   // bord subtil
			t.text = {223, 223, 223, 255};		   // #DFDFDF
			t.textDisabled = {125, 133, 144, 255}; // muted
			t.selection = {31, 111, 235, 200};	   // #1F6FEB (semi)
			t.accent = {31, 111, 235, 255};		   // #1F6FEB
			t.track = {13, 17, 23, 255};
			t.tabBar = {25, 29, 35, 255}; // barre d'onglets #191D23
			t.tab = {25, 29, 35, 255};	  // onglet inactif #191D23
			t.tabHover = {33, 39, 48, 255};
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
				// et fermera elle-meme via RequestClose() le moment venu.
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
				// Num3..Num6 et la virgule : indispensables aux raccourcis du
				// launcher (Ctrl+3..6, Ctrl+,) qui etaient annonces dans les
				// Parametres sans qu'aucune touche ne leur soit acheminee.
				case NkKey::NK_NUM3:
					mUI.input.SetKey(NkGuiKey::Num3, down);
					break;
				case NkKey::NK_NUM4:
					mUI.input.SetKey(NkGuiKey::Num4, down);
					break;
				case NkKey::NK_NUM5:
					mUI.input.SetKey(NkGuiKey::Num5, down);
					break;
				case NkKey::NK_NUM6:
					mUI.input.SetKey(NkGuiKey::Num6, down);
					break;
				case NkKey::NK_COMMA:
					mUI.input.SetKey(NkGuiKey::Comma, down);
					break;
				case NkKey::NK_MINUS:
					mUI.input.SetKey(NkGuiKey::Minus, down);
					break; // Ctrl+- : dézoom éditeur
				case NkKey::NK_EQUALS:
					mUI.input.SetKey(NkGuiKey::Equal, down);
					break; // Ctrl+= / Ctrl++ : zoom éditeur
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

			const float32 titleH = mUI.ItemHeight() + mUI.S(10.f); // barre de titre legerement plus grande
			mUI.titleBarH = titleH;								   // l'ecran de demarrage doit commencer en dessous
			const float32 toolbarH =
				(mToolbarFn && !fullScreen) ? mUI.S(46.f) : 0.f; // combos labellises (vue principale IDE)
			const float32 footerH = fullScreen ? 0.f : mUI.S(22.f);
			const float32 activityW = mUI.S(48.f);

			// Barre de titre custom UNE ligne : logo + menus | infos | min/max/close.
			DrawTitleBar(ec, {0.f, 0.f, W, titleH});
			// Barre d'outils Visual Studio (config/plateforme cible + Build/Run + emulateur).
			if (mToolbarFn && !fullScreen)
				DrawToolbar(ec, {0.f, titleH, W, toolbarH});

			const float32 bodyTop = titleH + toolbarH;
			const float32 bodyH = H - bodyTop - footerH;

			// MODALE : quand Preferences est ouvert, le corps (panneaux/editeur)
			// ne doit pas reagir aux clics/molette/frappes destines au popup. On
			// masque l'input pour le corps puis on le restaure pour DrawPreferences.
			// IDEM quand la souris est au-dessus d'un menu DEROULANT ouvert (deja
			// dessine + gere dans la barre de titre ci-dessus) : sinon l'editeur /
			// les zones a hit-test « brut » derriere le menu recoivent les clics.
			bool overPopup = false;
			for (int32 i = 0; i < mUI.popupDepth; ++i)
				if (nkgui::NkGuiRectContains(mUI.popupRects[i], mUI.input.mousePos)) {
					overPopup = true;
					break;
				}
			const bool modal = mShowPrefs || mUI.appModal || overPopup || mCtxOpen;
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
			}

			if (fullScreen) {
				// Launcher : remplace barre d'activite + dock + panneaux.
				mStartScreenFn(ec, mStartScreenUser);
			} else {
				DrawActivityBar({0.f, bodyTop, activityW, bodyH});
				DrawActivityBarRight({W - activityW, bodyTop, activityW, bodyH}); // IA (panneau droit)
				DockSpace(mUI, "##EditorDock", {activityW, bodyTop, W - activityW * 2.f, bodyH});
				// Seul le panneau CENTRAL masque la barre d'onglets de sa feuille quand il
				// est seul (il affiche ses propres onglets de fichiers) ; Terminal/Sortie/
				// sidebars gardent TOUJOURS leurs onglets, même seuls (façon VSCode).
				for (int32 i = 0; i < mNumPanels; ++i)
					if (mPanels[i])
						DockWindowHideSingleTab(mUI, mPanels[i]->Title(),
												mPanels[i]->DefaultSide() == NkEditorDockSide::NK_CENTER);
				BootstrapDocking();
				DrawPanels(ec);
				DrawStatusBar(footerH);
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
		void NkEditorShell::DrawTitleBar(NkEditorFrameContext &ec, const NkRect &bar) noexcept {
			auto &dl = mUI.dl;
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
			if (mTitleLogoTex) {
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
			const float32 bw = mUI.S(42.f);
			const NkRect cClose = {bar.x + bar.w - bw, bar.y, bw, bar.h};
			const NkRect cMax = {bar.x + bar.w - bw * 2.f, bar.y, bw, bar.h};
			const NkRect cMin = {bar.x + bar.w - bw * 3.f, bar.y, bw, bar.h};

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
				dl.AddText(mUI.font->Face(), mUI.font->TexId(), {ix, by}, info, {150, 150, 150, 255});
			}

			bool consumed = false;
			// Chip arrondi inset (style design) pour le fond de survol des controles.
			auto chip = [&](const NkRect &r) -> NkRect {
				const float32 vy = mUI.S(5.f), hx = mUI.S(3.f);
				return {r.x + hx, r.y + vy, r.w - 2.f * hx, r.h - 2.f * vy};
			};
			const float32 cround = mUI.S(4.f);
			// Minimiser (trait).
			{
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
			{
				const bool h = inR(cMax);
				if (h)
					dl.AddRectFilled(chip(cMax), hovBg, cround);
				const float32 gx = cMax.x + cMax.w * 0.5f, s = mUI.S(9.f);
				// ── DEUX glyphes franchement distincts ──────────────────────
				//
				// Le bouton doit dire d'un coup d'oeil dans quel etat on est.
				// L'ancien dessin opposait UN carre a DEUX carres decales de
				// 2 px a peine, pour un cote de 9 px : a l'oeil, c'etait le
				// meme. On ecarte donc nettement (decalage `d`), et le carre
				// arriere n'est plus qu'une EQUERRE (haut + droite), ce qui est
				// le glyphe « restaurer » universellement reconnu.
				//
				// L'ancienne version peignait un rectangle de couleur `bg` pour
				// « effacer » sous le carre avant. Au SURVOL, le fond reel est
				// `hovBg` : l'effacement laissait une trace visible. On dessine
				// desormais l'equerre en deux traits — plus rien a effacer, et
				// le rendu est correct quel que soit le fond.
				if (mWindow.IsMaximized()) {
					const float32 d = mUI.S(3.f);		 // ecart entre les deux plans
					const float32 c = s - d;			 // cote du carre avant
					const float32 x0 = gx - s * 0.5f;	 // bord gauche du carre avant
					const float32 y0 = cy - s * 0.5f + d; // bord haut du carre avant
					// Carre AVANT, en entier.
					dl.AddRect({x0, y0, c, c}, fg, 1.f);
					// Carre ARRIERE, reduit a son equerre visible : trait du
					// haut, puis trait de droite.
					dl.AddLine({x0 + d, y0 - d}, {x0 + d + c, y0 - d}, fg, 1.f);
					dl.AddLine({x0 + d + c, y0 - d}, {x0 + d + c, y0 - d + c}, fg, 1.f);
				} else {
					dl.AddRect({gx - s * 0.5f, cy - s * 0.5f, s, s}, fg, 1.f);
				}
				if (h && mUI.input.mouseClicked[0]) {
					mWindow.Maximize();
					consumed = true;
				}
			}
			// Fermer (X, survol rouge #f85149 arrondi).
			{
				const bool h = inR(cClose);
				if (h)
					dl.AddRectFilled(chip(cClose), {248, 81, 73, 255}, cround);
				const NkColor xc = h ? NkColor{255, 255, 255, 255} : fg;
				const float32 gx = cClose.x + cClose.w * 0.5f, s = mUI.S(5.f);
				dl.AddLine({gx - s, cy - s}, {gx + s, cy + s}, xc, 1.2f);
				dl.AddLine({gx - s, cy + s}, {gx + s, cy - s}, xc, 1.2f);
				if (h && mUI.input.mouseClicked[0]) {
					RequestQuit(); // vetoable : l'app peut demander confirmation
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
			{
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
					RequestQuit(/*windowClose=*/false); // quitter l'application
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
				const int32 act = NkCtxMenuDraw(mUI, mCtxSub, sitems, sen, sn);
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
				if (Begin(mUI, p->Title(), p->OpenPtr())) {
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
