#pragma once

// =============================================================================
// NkWindow.h
// Classe publique Window — implémentation concrète.
// Les membres platform-spécifiques sont encapsulés dans NkWindowData,
// défini par le backend platform (NkWin32Window.h, NkXLibWindow.h…).
//
// Usage :
//   nkentseu::NkInitialise();
//   nkentseu::NkWindowConfig cfg; cfg.title = "Hello";
//   nkentseu::NkWindow window(cfg);
//   while (window.IsOpen()) {
//       nkentseu::NkEvents().PollEvents();
//       /* render */
//   }
//   nkentseu::NkClose();
// =============================================================================

#include "NkWindowConfig.h"
#include "NKEvent/NkSafeArea.h"
#include "NkSurface.h"
#include "NKEvent/NkWindowId.h"
#include "NKEvent/NkSystemEvent.h" // NkDisplayInfo, NkDisplayChange (�num�ration moniteurs)
#include "NKContainers/Sequential/NkVector.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKMath/NKMath.h"
#include <string>

// Platform-specific NkWindowData struct definition
#if defined(NKENTSEU_PLATFORM_UWP)
#include "NKWindow/Platform/UWP/NkUWPWindow.h"
#elif defined(NKENTSEU_PLATFORM_XBOX)
#include "NKWindow/Platform/Xbox/NkXboxWindow.h"
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#include "NKWindow/Platform/Win32/NkWin32Window.h"
#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
#include "NKWindow/Platform/Noop/NkNoopWindow.h"
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#include "NKWindow/Platform/Wayland/NkWaylandWindow.h"
#elif defined(NKENTSEU_WINDOWING_XCB)
#include "NKWindow/Platform/XCB/NkXCBWindow.h"
#elif defined(NKENTSEU_WINDOWING_XLIB)
#include "NKWindow/Platform/XLib/NkXLibWindow.h"
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include "NKWindow/Platform/Android/NkAndroidWindow.h"
#elif defined(NKENTSEU_PLATFORM_MACOS)
#include "NKWindow/Platform/Cocoa/NkCocoaWindow.h"
#elif defined(NKENTSEU_PLATFORM_IOS)
#include "NKWindow/Platform/UIKit/NkUIKitWindow.h"
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#include "NKWindow/Platform/Emscripten/NkEmscriptenWindow.h"
#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
#include "NKWindow/Platform/HarmonyOS/NkHarmonyWindow.h"
#else
#include "NKWindow/Platform/Noop/NkNoopWindow.h"
#endif

namespace nkentseu {

	class NkEventSystem;

	// ---------------------------------------------------------------------------
	// NkClipboardImage — image du presse-papiers en pixels bruts.
	//
	// RGBA8 nu (width*height*4 octets, lignes du HAUT vers le bas) et non un
	// NkImage : NKWindow ne dépend PAS de NKImage (le fenêtrage n'a pas à tirer
	// les 12 codecs). Même choix que NkDropImageData côté NKEvent. L'application
	// qui veut un NkImage recopie les pixels (NkImage::Create + memcpy).
	// ---------------------------------------------------------------------------
	struct NkClipboardImage {
			uint32 width = 0;
			uint32 height = 0;
			NkVector<uint8> pixels; ///< RGBA8, taille = width * height * 4

			bool IsValid() const {
				return width > 0 && height > 0 && pixels.Size() == static_cast<usize>(width) * height * 4u;
			}
	};

	// ---------------------------------------------------------------------------
	// NkWindow — façade de fenêtre cross-plateforme
	// ---------------------------------------------------------------------------

	class NkWindow {
		public:
			NkWindow();
			explicit NkWindow(const NkWindowConfig &config);
			~NkWindow();

			NkWindow(const NkWindow &) = delete;
			NkWindow &operator=(const NkWindow &) = delete;
			NkWindow(NkWindow &&) = default;
			NkWindow &operator=(NkWindow &&) = default;

			// --- Cycle de vie ---
			bool Create(const NkWindowConfig &config);
			void Close();
			bool IsOpen() const;
			bool IsValid() const;

			// --- Identifiant ---
			NkWindowId GetId() const {
				return mId;
			}

			// --- Propriétés ---
			NkString GetTitle() const;
			void SetTitle(const NkString &title);
			math::NkVec2u GetSize() const;
			math::NkVec2u GetPosition() const;
			float32 GetDpiScale() const;
			math::NkVec2u GetDisplaySize() const;
			math::NkVec2u GetDisplayPosition() const;
			NkError GetLastError() const;
			NkWindowConfig GetConfig() const;

			// --- Moniteurs / Display (hot-plug + DPI runtime) ---
			// �num�re tous les moniteurs connect�s. Recalcule � chaque appel
			// (refl�te l'�tat courant � utile apr�s un NkSystemDisplayEvent de
			// hot-plug). Le premier �l�ment n'est pas garanti d'�tre le primaire :
			// utiliser NkDisplayInfo::isPrimary pour l'identifier.
			NkVector<NkDisplayInfo> EnumerateMonitors() const;
			// Moniteur qui contient (majoritairement) cette fen�tre. Sur les
			// plateformes mono-�cran (mobile/web) retourne l'�cran courant.
			NkDisplayInfo GetCurrentMonitor() const;
			// Nombre de moniteurs connect�s (>= 1 si au moins un �cran).
			uint32 GetMonitorCount() const;

			// --- Manipulation ---
			void SetSize(uint32 width, uint32 height);

			void SetSize(const math::NkVec2u &size) {
				SetSize(size.x, size.y);
			}

			void SetPosition(int32 x, int32 y);

			void SetPosition(const math::NkVec2u &pos) {
				SetPosition(pos.x, pos.y);
			}

			void SetVisible(bool visible);
			void Minimize();
			void Maximize();
			void Restore();
			bool IsMaximized() const; ///< true si la fenetre est maximisee (pour barre de titre custom)
			/// true si la fenetre est REDUITE (barre des taches). A interroger avant
			/// de rendre/redimensionner : une fenetre reduite garde un rect de
			/// placeholder non nul (~160x28 sous Windows), tester la taille ne
			/// detecte donc jamais la minimisation.
			bool IsMinimized() const;
			void BeginDragMove();	  ///< hand-off natif du deplacement (barre de titre custom, fenetre sans bordure)
			// Presse-papiers texte (UTF-8). Win32 = vrai presse-papiers OS ; autres
			// plateformes = fallback interne a l'application (copier/coller intra-app).
			void SetClipboardText(const NkString &text);
			NkString GetClipboardText() const;
			/// Bord de redimensionnement pour BeginResize (fenetre sans bordure).
			enum class NkResizeEdge { Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
			void BeginResize(NkResizeEdge edge); ///< hand-off natif du redimensionnement par un bord
			void SetFullscreen(bool fullscreen);

			// ── Decoration de la fenetre (bordure + barre de titre de l'OS) ──
			//
			// A l'execution, comme toute autre propriete de fenetre. Elle
			// n'etait reglable qu'a la CREATION (NkWindowConfig::frame), et sous
			// X11 elle etait meme purement ignoree : une application a barre de
			// titre custom se retrouvait avec DEUX jeux de boutons, les siens et
			// ceux du gestionnaire de fenetres.
			//
			// Mise en oeuvre : _MOTIF_WM_HINTS sous X11 (XLib/XCB),
			// zxdg_toplevel_decoration_v1 sous Wayland, style de fenetre sous
			// Win32. Sans effet la ou la notion n'existe pas (mobile, Web).
			void SetDecorated(bool decorated);
			bool IsDecorated() const;

			// ── Fenêtre discrète (outil flottant type tableau de références) ──
			//
			// Trois propriétés pensées ensemble : une fenêtre d'appoint qui reste
			// visible au-dessus du logiciel principal (topmost), qu'on estompe pour
			// voir au travers (opacité), voire qu'on laisse traverser par la souris
			// pour travailler dessous (click-through). Réglables aussi à la
			// CRÉATION via NkWindowConfig (évite le clignotement d'une fenêtre
			// créée « normale » puis corrigée une frame plus tard).
			//
			// Support par plateforme (l'intention est TOUJOURS mémorisée dans la
			// config pour que les getters restent cohérents) :
			//   - opacité      : Win32 (WS_EX_LAYERED + LWA_ALPHA), X11 XLib/XCB
			//                    (_NET_WM_WINDOW_OPACITY, respecté par les WM/
			//                    compositeurs courants), macOS (NSWindow.alphaValue).
			//                    Wayland : pas de protocole client → sans effet.
			//   - toujours-devant : Win32 (HWND_TOPMOST), X11 (_NET_WM_STATE_ABOVE),
			//                    macOS (NSFloatingWindowLevel). Wayland : pas de
			//                    protocole → sans effet.
			//   - click-through : Win32 (WS_EX_TRANSPARENT), macOS
			//                    (ignoresMouseEvents), Wayland (input region vide).
			//                    X11 : demande l'extension Shape (non liée) → sans
			//                    effet, chantier noté dans la ROADMAP.
			//   - mobile / Web / consoles : notions inexistantes → sans effet.

			/// Opacité globale de la fenêtre, contenu compris. [0..1], borné ;
			/// 1 = opaque. Indépendant de NkWindowConfig::transparent (qui, lui,
			/// rend le FOND composité : les deux se combinent).
			void SetOpacity(float32 opacity);
			float32 GetOpacity() const;

			/// La fenêtre reste au-dessus de toutes les fenêtres normales,
			/// même sans focus. L'utilisateur doit pouvoir le désactiver
			/// (c'est une option d'outil, jamais un état imposé).
			void SetAlwaysOnTop(bool onTop);
			bool IsAlwaysOnTop() const;

			/// La souris TRAVERSE la fenêtre : clics, molette et survol vont à la
			/// fenêtre du dessous. À coupler avec une opacité réduite, sinon la
			/// fenêtre semble « morte ». Prévoir une sortie clavier/raccourci
			/// global : une fois transparente aux clics, la fenêtre ne reçoit
			/// plus l'événement souris qui permettrait de la re-solidifier.
			void SetClickThrough(bool clickThrough);
			bool IsClickThrough() const;

			// ── Presse-papiers image (RGBA8 brut, cf. NkClipboardImage) ──
			//
			// Complète le presse-papiers texte ci-dessus, même philosophie :
			// Win32 = vrai presse-papiers OS (CF_DIBV5/CF_DIB, alpha préservé
			// quand la source le fournit) ; autres plateformes = fallback interne
			// à l'application (copier/coller intra-app) en attendant les
			// implémentations OS (X11 CLIPBOARD image/png, NSPasteboard,
			// wl_data_device — notées dans la ROADMAP).
			//
			// Limite Win32 assumée : les sources qui ne posent QU'un PNG (sans
			// DIB) ne sont pas lues — décoder du PNG exigerait NKImage, que
			// NKWindow ne tire pas. Dans la pratique, capture d'écran Windows et
			// navigateurs posent aussi un DIB.

			/// Dépose une image dans le presse-papiers. Retourne false si l'image
			/// est invalide ou si l'OS refuse.
			bool SetClipboardImage(const NkClipboardImage &image);
			/// Lit l'image du presse-papiers vers `out`. Retourne false s'il n'y
			/// a pas d'image (out est alors laissée vide/invalide).
			bool GetClipboardImage(NkClipboardImage &out) const;
			/// Vrai s'il y a une image disponible — test léger, sans copier les
			/// pixels (pour activer/griser un « Coller »).
			bool HasClipboardImage() const;

			bool SupportsOrientationControl() const;
			void SetScreenOrientation(NkScreenOrientation orientation);
			NkScreenOrientation GetScreenOrientation() const;
			void SetAutoRotateEnabled(bool enabled);
			bool IsAutoRotateEnabled() const;

			// --- Android specifics ---
			void SetHideSystemUI(bool hide); // Masquer status bar + navigation bar
			bool GetHideSystemUI() const;
			void SetLockOrientation(bool lock); // Emp�cher la rotation
			bool GetLockOrientation() const;

			// --- Souris ---
			void SetMousePosition(uint32 x, uint32 y);

			void SetMousePosition(const math::NkVec2u &pos) {
				SetMousePosition(pos.x, pos.y);
			}

			void ShowMouse(bool show);
			void CaptureMouse(bool capture);

			// --- Curseur ---
			// Forme du curseur dans la zone client (consomm� par les UI : poign�e de
			// redimensionnement, lien cliquable, champ texte...). Mapp� sur les
			// curseurs natifs (Win32 IDC_*, etc.). No-op sur mobile/web (sans curseur).
			enum class NkCursorType {
				Arrow = 0,	///< fl�che standard
				TextInput,	///< I-beam (saisie texte)
				Hand,		///< main (lien)
				ResizeNS,	///< redimensionnement vertical  ↕
				ResizeWE,	///< redimensionnement horizontal ↔
				ResizeNWSE, ///< diagonale ↘↖
				ResizeNESW	///< diagonale ↗↙
			};
			// Persistant : � rappeler chaque frame avec le curseur voulu (sinon, sur
			// certaines plateformes, le syst�me le r�initialise � la fl�che).
			void SetCursor(NkCursorType cursor);

			// Empeche le curseur de sortir de la zone client de la fenetre
			// (clip rectangulaire). Indispensable pour les FPS / RTS qui
			// veulent tenir la souris en jeu sans qu'elle parte sur un autre
			// ecran. Cross-platform : implem native quand possible (Win32
			// ClipCursor, XLib XGrabPointer, ...), no-op sur les plateformes
			// sans curseur (mobile / web tactile).
			void ClipMouseToClient(bool clip);

			// --- Clavier logiciel (mobile : iOS / Android) ---
			// Type de clavier logiciel demand� � influe sur la disposition des
			// touches propos�es par l'OS (mapp� sur UIKeyboardType iOS / inputType
			// Android). Ignor� sur les plateformes � clavier physique.
			enum class NkSoftKeyboardType {
				Default = 0, ///< clavier alphanum�rique standard
				Ascii,		 ///< ASCII uniquement (identifiants, code)
				Number,		 ///< pav� num�rique (chiffres)
				Phone,		 ///< pav� t�l�phone
				Email,		 ///< optimis� e-mail (� @ � accessible)
				Url,		 ///< optimis� URL (� / � � .com � accessibles)
				Decimal		 ///< num�rique avec s�parateur d�cimal
			};
			// Libell� / s�mantique de la touche de retour du clavier logiciel.
			enum class NkSoftKeyboardReturnKey { Default = 0, Done, Go, Next, Search, Send };

			// Options d'un clavier logiciel (valeurs par d�faut = saisie de texte
			// g�n�rique avec correction automatique).
			struct NkSoftKeyboardConfig {
					NkSoftKeyboardType type = NkSoftKeyboardType::Default;
					NkSoftKeyboardReturnKey returnKey = NkSoftKeyboardReturnKey::Default;
					bool autocorrect = true;	 ///< correction/suggestions automatiques
					bool autocapitalize = false; ///< majuscule auto en d�but de phrase
					bool secure = false;		 ///< champ mot de passe (saisie masqu�e)
			};

			// Affiche le clavier logiciel (mobile) et d�marre une session de saisie.
			// La saisie est ensuite d�livr�e par le syst�me d'�v�nements :
			//   - NkTextInputEvent  pour chaque caract�re Unicode (via GetUtf8/GetCodepoint) ;
			//   - NkKeyPressEvent/NkKeyReleaseEvent pour NK_BACK (retour arri�re),
			//     NK_ENTER (retour) et NK_TAB (tabulation).
			// � appeler quand un champ texte prend le focus. No-op sur desktop
			// (clavier physique) et sur les backends sans clavier logiciel.
			// NB : pas de d�faut `= {}` ici � clang-mingw (Windows) refuse un
			// brace-init d'une struct imbriqu�e � initialiseurs par d�faut tant que
			// la classe englobante est incompl�te ("default member initializer for
			// 'type' needed within definition of enclosing class"). Pour un appel
			// "d�fauts", passer explicitement `NkSoftKeyboardConfig{}`.
			void ShowSoftKeyboard(const NkSoftKeyboardConfig &config);
			// Masque le clavier logiciel et termine la session de saisie. No-op
			// l� o� ShowSoftKeyboard l'est.
			void HideSoftKeyboard();
			// true si le clavier logiciel est actuellement affich� par cette fen�tre.
			bool IsSoftKeyboardVisible() const;

			// --- Web / WASM ---
			void SetWebInputOptions(const NkWebInputOptions &options);
			NkWebInputOptions GetWebInputOptions() const;

			// --- OS extras ---
			void SetProgress(float progress);

			// --- Safe Area (mobile) ---
			NkSafeAreaInsets GetSafeAreaInsets() const;

			// --- Surface graphique ---
			NkSurfaceDesc GetSurfaceDesc() const;

			// Platform data — accessed directly by backend .cpp files
			struct NkWindowData mData;

			// Config accessible aux backends (.cpp plateforme), comme mData : permet
			// aux callbacks libres (ex. Wayland configure) de synchroniser la config.
			NkWindowConfig &ConfigData() noexcept {
				return mConfig;
			}

		private:
			NkWindowId mId = 0;
			bool mIsOpen = false;
			NkWindowConfig mConfig;
			NkError mLastError;
	};

} // namespace nkentseu
