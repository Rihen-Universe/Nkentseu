#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorShell.h
// @Brief   Coquille d'application d'editeur : fenetre + docking + panneaux.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// NkEditorShell est la base reutilisable des editeurs Nkentseu (NKCode = IDE,
// et plus tard Nogee = editeur de moteur). Elle POSSEDE :
//   - la fenetre (NKWindow) + la cible de rendu (NKCanvas/NkRenderWindow),
//   - le contexte NKGui (qui contient fenetres/dock/layout/police/draw lists),
//   - la barre de menus (Fichier/Affichage/Fenetre + menu applicatif optionnel),
//   - la palette de commandes (Ctrl+P),
//   - la boucle principale (events -> frame -> docking -> panneaux -> rendu).
//
// L'application N'A QU'A : creer le shell, enregistrer ses panneaux/commandes,
// appeler Run(). Pipeline NKGui 2D pur (NKCanvas), independant du moteur 3D.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorCommand.h"

#include "NKWindow/NKWindow.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkClock.h"
#include "NKGui/NKGui.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKEditorKit/NkFontPrefs.h"
#include "NKEditorKit/NkIEditorRenderer.h" // backend de rendu pluggable

namespace nkentseu {
	namespace editorkit {

		// Parametres de creation du shell.
		struct NKEDITORKIT_API NkEditorShellConfig {
				const char *title = "Nkentseu Editor";
				uint32 width = 1280;
				uint32 height = 720;
				bool resizable = true;
				NkEditorGfxApi graphicsApi = NkEditorGfxApi::Auto;
				// Backend de rendu INJECTE (optionnel). nullptr => impl NKCanvas par
				// defaut (IDE). Une app 2D/3D (anim/moteur) fournit ici une impl NKRHI/
				// NKRenderer. Le shell NE POSSEDE PAS un renderer injecte (l'app le gere).
				NkIEditorRenderer *renderer = nullptr;
		};

		// Menu applicatif optionnel : appele a l'interieur de la barre de menus,
		// apres « Fenetre », pour que l'app ajoute ses propres menus.
		using NkEditorAppMenuFn = void (*)(NkEditorFrameContext &ec, void *user);

		class NKEDITORKIT_API NkEditorShell {
			public:
				static constexpr int32 MAX_PANELS = 64;
				static constexpr int32 MAX_COMMANDS = 128;

				NkEditorShell() = default;
				~NkEditorShell();

				NkEditorShell(const NkEditorShell &) = delete;
				NkEditorShell &operator=(const NkEditorShell &) = delete;

				// ── Cycle de vie ────────────────────────────────────────────────────
				bool Init(const NkEditorShellConfig &config) noexcept;
				int Run() noexcept; ///< boucle bloquante ; retourne le code de sortie

				void RequestClose() noexcept {
					mRunning = false;
				}

				// ── Enregistrement (le shell NE POSSEDE PAS les panneaux) ───────────
				bool AddPanel(NkEditorPanel *panel) noexcept;
				bool RegisterCommand(const char *name, NkEditorCommandFn fn, void *user = nullptr,
									 const char *shortcut = "") noexcept;

				void SetAppMenu(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mAppMenuFn = fn;
					mAppMenuUser = user;
				}

				// Items injectes DANS le menu « Fichier » (avant Palette/Quitter) : l'app
				// y met Nouveau fichier/projet/workspace, Enregistrer(/sous/tout), Deploiement.
				void SetFileMenu(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mFileMenuFn = fn;
					mFileMenuUser = user;
				}

				// Barre d'outils horizontale (sous la barre de titre, facon Visual Studio) :
				// l'app y dessine config/plateforme cible + bouton Build/Run + emulateur.
				void SetToolbar(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mToolbarFn = fn;
					mToolbarUser = user;
				}

				// Overlay applicatif (dessine APRES les panneaux, sur dlOverlay) : l'app y
				// rend ses dialogues modaux (creation de projet, proprietes...). Quand
				// ctx.appModal est leve, le shell masque l'input du corps. L'input du popup
				// est restaure avant l'appel (comme la fenetre Preferences).
				void SetOverlay(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mOverlayFn = fn;
					mOverlayUser = user;
				}

				// Ecran de demarrage (launcher) : dessine TOUT le corps quand
				// ctx.appFullScreen est leve (remplace barre d'outils + panneaux).
				void SetStartScreen(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mStartScreenFn = fn;
					mStartScreenUser = user;
				}

				// Maximise la fenetre (ex. au lancement, pour l'ecran de demarrage).
				void Maximize() noexcept {
					mWindow.Maximize();
				}

				// Donne une taille/position de fenetre (non maximisee).
				void Resize(uint32 w, uint32 h) noexcept {
					mWindow.SetSize(w, h);
				}

				float32 DpiScale() const noexcept {
					const float32 s = mUI.S(1.f);
					return s > 0.5f ? s : 1.f;
				} ///< echelle DPI (pour uploader les icones a la taille ecran)

				// Upload une image RGBA8 comme texture backend ; renvoie un texId stable
				// (0 si echec). Pour les logos/icones de l'app (dessines via AddImage).
				uint32 UploadRGBA(const uint8 *pixels, int32 w, int32 h) noexcept {
					if (!mRenderer || !pixels || w <= 0 || h <= 0)
						return 0;
					const uint32 id = mNextTexId++;
					return mRenderer->UploadImageRGBA(id, pixels, w, h) ? id : 0;
				}

				// Logo dessine a gauche de la barre de titre (texId via UploadRGBA).
				// aspect > 0 : LOGO COMPLET (wordmark icone+nom) dessine en preservant son
				// ratio largeur/hauteur ; "nkcode" n'est alors PAS re-ecrit a cote (deja dans
				// l'image). aspect == 0 : logo carre (icone seule).
				void SetTitleLogo(uint32 texId, float32 aspect = 0.f) noexcept {
					mTitleLogoTex = texId;
					mTitleLogoAspect = aspect;
				}

				// ── Layout ──────────────────────────────────────────────────────────
				void ResetLayout() noexcept {
					mDockBootstrap = true;
				}

				// Persistance de disposition : à recâbler sur la sérialisation d'arbre NKGui
				// (TODO) ; stubs no-op en attendant pour conserver l'API.
				void SaveLayout(const char *path) noexcept {
					(void)path;
				}

				bool LoadLayout(const char *path) noexcept {
					(void)path;
					return false;
				}

				// Etat d'interface PAR PROJET (lu/ecrit dans un fichier de config du
				// workspace, ex. <ws>/.nkcode/ui.cfg) : fenetre maximisee + panneaux
				// ouverts. LoadUiState applique l'etat ; no-op si le fichier est absent.
				void LoadUiState(const char *path) noexcept;
				void SaveUiState(const char *path) noexcept;

				// ── Barre d'etat (footer VSCode) : texte gauche/droite mis par l'app ─
				void SetFooter(const char *left, const char *right = "") noexcept;
				// ── Zoom de l'editeur (Ctrl+molette / Ctrl+± ) : ajuste la taille de la
				//    police du code (+ terminal), reconstruit l'atlas, persiste. ──────────
				void NudgeCodeFontSize(float32 delta) noexcept;
				void ResetCodeFontSize() noexcept;	   ///< remet la police du code à la taille par défaut (Ctrl+0)
				float32 CodeFontSize() const noexcept; ///< taille GLOBALE par défaut (prefs)
				static constexpr float32 kDefaultCodeFontSize = 15.f; ///< défaut (= NkFontPrefs::codeSize)
				static constexpr float32 kCodeReloadDebounce =
					0.12f; ///< délai (s) avant rebuild de l'atlas code après le dernier cran de zoom

				// ── Zoom PAR ONGLET (per-file) : l'app (qui possède les onglets) enregistre un
				//    handler ; Nudge/Reset le routent vers lui (delta, ou reset=true) au lieu
				//    d'agir sur la taille globale. L'app pilote ensuite l'atlas via RequestCodeSize.
				using NkZoomFn = void (*)(void *user, float32 delta, bool reset);

				void SetZoomHandler(NkZoomFn fn, void *user) noexcept {
					mZoomFn = fn;
					mZoomUser = user;
				}

				// CACHE d'atlas de code PAR TAILLE : l'editeur appelle EnsureCodeSize (arme la
				// rasterisation d'une taille) + CodeFontForSize (police a utiliser MAINTENANT, non
				// bloquant). Une taille deja rasterisee reste en cache -> revenir sur un onglet zoome
				// = atlas DEJA pret (aucun rebuild, aucun « saut »).
				void EnsureCodeSize(float32 logicalPx, bool immediate = false) noexcept;
				nkgui::NkGuiFont *CodeFontForSize(float32 logicalPx) noexcept;
				float32 ActiveCodeSize() const noexcept; ///< taille de code actuellement affichée (indicateur)
				// Police du TERMINAL : atlas PROPRE a taille GLOBALE fixe, decouple du zoom
				// par-onglet de l'editeur (le terminal ne bouge pas quand on zoome un fichier).
				nkgui::NkGuiFont *TermCodeFont() noexcept;
				void RequestTermSize(float32 logicalPx) noexcept; ///< zoom du TERMINAL (survol) : taille voulue (0 =
																  ///< globale), rebuild debounce
				float32 TermSize() const noexcept; ///< taille actuelle du terminal (pour calculer le zoom au survol)
				// ── Infos centrees dans la barre de titre (ex. fichier actif) ────────
				void SetTitleInfo(const char *center) noexcept;

				// ── Acces (pour besoins avances) ────────────────────────────────────
				nkgui::NkGuiContext &Ui() noexcept {
					return mUI;
				}

				// ── Preferences (menu dedie : Polices, Theme, ... extensible) ───────
				void OpenPreferences(int32 tab = 0) noexcept {
					mShowPrefs = true;
					mPrefsTab = tab;
					mPrefsJustOpened = true;
				}

			private:
				void LoadFontsFromPrefs() noexcept;	   ///< (re)charge mFont + mCodeFont
				void LoadUiFont() noexcept;			   ///< (re)charge SEULE la police d'interface
				void LoadCodeFont() noexcept;		   ///< (re)charge le repli mCodeFont (taille globale)
				void BuildCodeSlot(int32 px) noexcept; ///< rasterise une taille dans le cache d'atlas (LRU)
				void LoadTermFont() noexcept;		   ///< (re)charge la police du TERMINAL (taille globale fixe)
				void DrawPreferences(NkEditorFrameContext &ec) noexcept; ///< fenetre Preferences (categories)
				void BuildMenuBar(NkEditorFrameContext &ec, const nkgui::NkRect &rect) noexcept;
				void DrawTitleBar(NkEditorFrameContext &ec, const nkgui::NkRect &bar) noexcept;
				void DrawToolbar(NkEditorFrameContext &ec, const nkgui::NkRect &rect) noexcept;
				void HandleEdgeResize(float32 W, float32 H) noexcept;
				void DrawPanels(NkEditorFrameContext &ec) noexcept;
				void BootstrapDocking() noexcept;
				void DrawCommandPalette(NkEditorFrameContext &ec) noexcept;
				void ExecuteCommand(int32 index) noexcept;
				void HookEvents() noexcept;
				void MapEditKey(NkKey k, bool down) noexcept;
				void TryRunShortcut(NkKey k, bool shift) noexcept;
				void DrawActivityBar(const nkgui::NkRect &bar) noexcept;
				void DrawStatusBar(float32 footerH) noexcept;

				// === Redimensionnement / deplacement : hand-off NATIF (BeginResize/BeginDragMove).
				// Le hand-off bloque (boucle modale OS) -> on le DIFFERE en fin de boucle Run()
				// (jamais en plein milieu d'une frame, pour eviter la re-entrance de RenderFrame). ===
				void RenderFrame() noexcept; // rend UNE frame (appele par Run + le callback size/move)
				static void
				SizeMoveFrameThunk(void *user) noexcept; // callback OS : redessine pendant le resize (anti-stretch)
				int32 mPendingResizeEdge = -1;			 // -1 aucun ; sinon NkResizeEdge a declencher
				bool mPendingDragMove = false;			 // deplacement de barre de titre a declencher

				// === Fenetre / rendu ===
				NkWindow mWindow;
				NkIEditorRenderer *mRenderer = nullptr; // backend pluggable (NKCanvas par defaut / NKRHI injecte)
				uint32 mNextTexId = 0x4E4B0100u;		// ids de textures app (logos/icones) — distincts des polices
				uint32 mTitleLogoTex = 0;				// logo dans la barre de titre
				float32 mTitleLogoAspect = 0.f;			// >0 => wordmark (ratio l/h), pas de texte "nkcode"
				bool mOwnsRenderer = false;				// true => cree par le shell (a detruire)

				// === NKGui (contexte + police possedee) ===
				nkgui::NkGuiContext mUI;
				nkgui::NkGuiFont mFont;		   ///< police d'interface (Inter/Karla)
				nkgui::NkGuiFont mCodeFont;	   ///< police monospace de l'EDITEUR (taille = onglet actif, zoom)
				nkgui::NkGuiFont mTermFont;	   ///< police monospace du TERMINAL (taille GLOBALE fixe, non zoomee)
				float32 mTermLoadedSize = 0.f; ///< taille logique a laquelle mTermFont est construit
				float32 mTermTargetSize = 0.f; ///< taille logique voulue du terminal (0 = globale), zoom au survol
				float32 mTermReloadCountdown = -1.f; ///< debounce du rebuild de l'atlas terminal (zoom)
				bool mFontOk = false;
				bool mFontReloadPending = false; ///< reload des DEUX polices differe au debut de frame (anti-crash)
				float32 mCodeReloadCountdown =
					-1.f; ///< zoom : debounce (s). >=0 => rasterise la taille en attente quand il atteint 0
				float32 mCodeTargetSize = 0.f; ///< (init) taille logique de mCodeFont (repli global)
				float32 mCodeLoadedSize = 0.f; ///< taille logique a laquelle mCodeFont (repli) est construit
				float32 mCodeActiveLogical =
					0.f; ///< derniere taille demandee par l'editeur (indicateur de zoom ; 0 = globale)
				// Cache d'atlas de code par taille (px) : chaque taille rasterisee 1 seule fois.
				static constexpr int32 kCodeCacheN = 8;

				struct CodeSlot {
						nkgui::NkGuiFont *font = nullptr;
						int32 px = 0;
						uint32 lru = 0;
				};

				CodeSlot mCodeSlots[kCodeCacheN] = {};
				uint32 mCodeClock = 0;		///< horloge LRU du cache
				int32 mCodePendingPx = 0;	///< px a rasteriser quand le debounce expire
				NkZoomFn mZoomFn = nullptr; ///< handler zoom per-onglet (app)
				void *mZoomUser = nullptr;
				NkFontPrefs mFontPrefs;			   ///< reglages de polices (persistes)
				nkgui::NkGuiTheme mDefaultTheme;   ///< theme par defaut (vit dans l'app) -> Reinitialiser
				nkgui::NkGuiSyntax mDefaultSyntax; ///< couleurs langages par defaut -> Reinitialiser
				bool mShowPrefs = false;		   ///< fenetre Preferences ouverte ?
				bool mPrefsJustOpened = false;	   ///< grace 1 frame (anti auto-fermeture)
				int32 mPrefsTab = 0;			   ///< categorie active (0=Polices, 1=Theme)
				int32 mThemeSel = 6;			   ///< element de theme selectionne (defaut Accent)
				int32 mSynSel = 1;				   ///< token de langage selectionne (defaut Mot-cle)

				// === Panneaux / commandes ===
				NkEditorPanel *mPanels[MAX_PANELS] = {};
				int32 mNumPanels = 0;
				NkEditorCommand mCommands[MAX_COMMANDS] = {};
				int32 mNumCommands = 0;
				NkEditorAppMenuFn mAppMenuFn = nullptr;
				void *mAppMenuUser = nullptr;
				NkEditorAppMenuFn mFileMenuFn = nullptr;
				void *mFileMenuUser = nullptr;
				NkEditorAppMenuFn mToolbarFn = nullptr;
				void *mToolbarUser = nullptr;
				NkEditorAppMenuFn mOverlayFn = nullptr;
				void *mOverlayUser = nullptr;
				NkEditorAppMenuFn mStartScreenFn = nullptr;
				void *mStartScreenUser = nullptr;

				// === Palette de commandes ===
				bool mPaletteOpen = false;
				int32 mPaletteSel = 0;

				// === Barre de titre custom + footer + activity bar ===
				char mTitle[160] = {};
				char mTitleCenter[200] = {};
				char mFooterLeft[256] = {};
				char mFooterRight[128] = {};
				int32 mActivityIndex = 0; // icone selectionnee dans l'activity bar

				// === Etat boucle ===
				NkClock mClock;
				bool mRunning = true;
				bool mDockBootstrap = true;
				bool mPopupMasked = false;	  ///< input du corps masque (souris sur un popup) - cf. dockHeaderFn
				nkgui::NkGuiInput mRealInput; ///< input REEL pendant le masquage (barres d onglets)
				NkEditorGfxApi mGraphicsApi = NkEditorGfxApi::Auto;
				uint32 mLastWidth = 0;
				uint32 mLastHeight = 0;
				// Deplacement de fenetre par la barre de titre : arme au press, ne demarre
				// qu'apres un vrai glissement (seuil) -> un simple clic ne deplace jamais.
				bool mTitleDragArmed = false;
				float32 mDragStartX = 0.f, mDragStartY = 0.f;
		};

	} // namespace editorkit
} // namespace nkentseu
