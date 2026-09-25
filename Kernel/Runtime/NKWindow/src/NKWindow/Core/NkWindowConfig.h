// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkWindowConfig.h
// Configuration de création de fenêtre.
//
// Changement par rapport à la version précédente :
//   → Ajout du champ `surfaceHints` (NkSurfaceHints).
//     Ce champ est optionnel. Laisser vide pour Vulkan, DirectX, Metal,
//     Software, EGL. Rempli automatiquement par
//     NkGraphicsContextFactory::PrepareWindowConfig() pour OpenGL/GLX.
// =============================================================================

#include "NkTypes.h"
#include "NKEvent/NkSafeArea.h"
#include "NkSurfaceHint.h" // ← seul ajout d'include

namespace nkentseu {

	struct NkWebInputOptions {
			bool captureKeyboard = true;
			bool allowBrowserShortcuts = true;
			bool captureMouseMove = true;
			bool captureMouseLeft = true;
			bool captureMouseMiddle = true;
			bool captureMouseRight = false;
			bool captureMouseWheel = true;
			bool captureTouch = true;
			bool preventContextMenu = false;
	};

	// -------------------------------------------------------------------------
	// NkNativeWindowOptions
	// Options natives avancées (cross-backend via handles opaques).
	//
	// Interprétation des handles:
	//   - Windows : externalWindowHandle/parentWindowHandle = HWND
	//   - XLib    : externalWindowHandle/parentWindowHandle = ::Window
	//   - XCB     : externalWindowHandle/parentWindowHandle = xcb_window_t
	//   - Wayland : externalWindowHandle = non supporte, parentWindowHandle = xdg_toplevel*
	//   - macOS   : externalWindowHandle/parentWindowHandle = NSWindow*
	//   - iOS     : externalWindowHandle = UIWindow*, parentWindowHandle = UIView*
	//   - Android : externalWindowHandle = ANativeWindow*
	//   - Web     : externalWindowHandle = const char* (canvas selector, ex: "#canvas")
	//   - UWP/Xbox/Noop : externalWindowHandle = handle opaque
	//
	// externalDisplayHandle est utilisé uniquement quand nécessaire:
	//   - XLib : Display*
	//   - XCB  : xcb_connection_t*
	//   - Wayland : wl_display*
	//   - macOS : NSScreen*
	//   - iOS   : UIScreen* (création interne uniquement)
	//   - Android : android_app* (optionnel)
	//   - Web : const char* (canvas selector alternatif)
	// -------------------------------------------------------------------------
	struct NkNativeWindowOptions {
			bool useExternalWindow = false;
			uintptr externalWindowHandle = 0;
			uintptr externalDisplayHandle = 0;

			uintptr parentWindowHandle = 0;
			bool utilityWindow = false;

			// Win32 only: copy pixel format from this HWND before WGL context creation.
			uintptr win32PixelFormatShareWindowHandle = 0;
	};

	// ═══════════════════════════════════════════════════════════════════════
	// LE CONTRAT TAILLE / POSITION — ecrit le 25/09/2026, apres un defaut qui a
	// vecu des mois : `SetSize(GetSize())` n'etait PAS l'identite, et la fenetre
	// des editeurs grossissait de +16 px en largeur et +39 en hauteur A CHAQUE
	// LANCEMENT (ils sauvent leur geometrie a la fermeture et la restaurent au
	// demarrage). Signature d'un cote qui parle CLIENT et d'un autre FENETRE.
	//
	//   LA TAILLE EST TOUJOURS LA ZONE **CLIENT** — la surface ou l'on dessine,
	//   sans barre de titre ni bordure.
	//     `width` / `height`, `minWidth` / `minHeight`, `maxWidth` / `maxHeight`,
	//     `NkWindow::GetSize()`, `NkWindow::SetSize()`.
	//     C'est aussi la taille de la swapchain : demander 1280x720 donne
	//     1280x720 pixels a peindre, sur toutes les plateformes.
	//
	//   LA POSITION EST TOUJOURS LE COIN HAUT-GAUCHE DE LA **FENETRE** — cadre
	//   compris, parce que c'est le seul point que l'utilisateur voit et que le
	//   systeme sait placer.
	//     `x` / `y`, `NkWindow::GetPosition()`, `NkWindow::SetPosition()`.
	//
	// Consequences a connaitre :
	//   • `SetSize(GetSize())` est une IDENTITE. Dix cycles ne deplacent rien.
	//     C'est le critere que mesure `NkWindowSonde` (essai G).
	//   • une fenetre SANS CADRE a fenetre == client : la conversion ne doit
	//     RIEN ajouter, et c'est la garde qui manquait aux deux tiers des sites.
	//   • la conversion client -> fenetre n'existe qu'a UN endroit par dorsal.
	//     Si vous en ecrivez une deuxieme, vous reintroduisez ce defaut.
	// ═══════════════════════════════════════════════════════════════════════
	//
	// NkWindowConfig
	//
	// ⚠️ TOUS CES REGLAGES NE SONT PAS TENUS PAR TOUS LES DORSAUX, et c'est
	//    ecrit noir sur blanc : `wiki/Runtime/NKWindow/Proprietes-par-dorsal.md`
	//    donne la table de verite propriete x dorsal (agit / silence / refus
	//    nomme / sans objet). Elle existe parce qu'un utilisateur a pose
	//    `resizable = false`, vu sa fenetre se redimensionner quand meme, et
	//    cherche l'erreur chez lui pendant que le defaut etait chez nous.
	//
	// ⚠️ DEPUIS LE 25/09/2026, UN REGLAGE NON TENU LE DIT. Si la valeur
	//    demandee differe du defaut et que le dorsal ne l'honore pas, un REFUS
	//    NOMME sort au journal, une fois, avec le nom du champ et celui de la
	//    plateforme (voir `NkWindowAudit.h`). Si votre reglage ne produit rien
	//    et que le journal est muet, le defaut est ailleurs que dans ce struct.
	//
	// ⚠️ UN ACCESSEUR DECRIT LE MONDE, PAS NOTRE MEMOIRE. `IsAlwaysOnTop()`,
	//    `GetOpacity()`, `IsClickThrough()` rendent ce que la fenetre EST — donc
	//    `false` / `1.0` / `false` la ou la propriete n'a pas ete appliquee, et
	//    la ou il n'y a pas (ou plus) de fenetre native.
	// -------------------------------------------------------------------------
	struct NkWindowConfig {
			// --- Position et taille ---
			// x, y      : coin haut-gauche de la FENETRE (cadre compris)
			// width,    : taille de la zone CLIENT (surface dessinable)
			// height      — cf. LE CONTRAT ci-dessus. Ne pas melanger.
			int32 x = 100;
			int32 y = 100;
			uint32 width = 1280;
			uint32 height = 720;
			uint32 minWidth = 160;
			uint32 minHeight = 90;
			uint32 maxWidth = 0xFFFF;
			uint32 maxHeight = 0xFFFF;

			// --- Comportement ---
			bool centered = true;
			bool resizable = true;
			bool movable = true;
			bool closable = true;
			bool minimizable = true;
			bool maximizable = true;
			bool canFullscreen = true;
			bool fullscreen = false;
			bool modal = false;
			bool vsync = true;
			bool dropEnabled = false;
			NkScreenOrientation screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;

			// --- Apparence ---
			bool frame = true;
			bool hasShadow = true;
			bool transparent = false;
			bool visible = true;
			uint32 bgColor = 0x141414FF;

			// ── Fenêtre discrète (outils flottants type tableau de références) ──
			// Ces trois réglages existent aussi à l'exécution (SetAlwaysOnTop,
			// SetOpacity, SetClickThrough) ; les poser ici évite le clignotement
			// d'une fenêtre créée « normale » puis corrigée une frame plus tard.
			// Support par plateforme : voir la doc des méthodes dans NkWindow.h.
			bool alwaysOnTop = false;  ///< reste au-dessus des autres fenêtres
			bool clickThrough = false; ///< transparente aux clics (la souris traverse)
			float32 opacity = 1.0f;	   ///< opacité globale [0..1], 1 = opaque

			// ── FENETRE QUI NE PREND PAS LE FOCUS (25/09) ────────────────────
			// Elle s'affiche, elle se rend, mais elle ne devient PAS la fenetre active :
			// le clavier et les clics restent a ce que l'utilisateur faisait. C'est le
			// besoin des fenetres de MESURE.
			//
			// POURQUOI (deux faux defauts en une nuit, 24/09) : une sonde qui passe au
			// premier plan recoit les clics et les frappes de l'utilisateur. Une demande
			// est partie d'un composeur que personne n'avait valide ; un presse-papiers a
			// ete ecrit, et on en a conclu a tort que l'envoi copiait. **Une sonde qui
			// recoit de l'entree humaine ne mesure plus le produit : elle fabrique des
			// defauts qui n'existent pas.**
			//
			// ⚠️ CE N'EST PAS `clickThrough`. Celle-la laisse la souris TRAVERSER la
			//    fenetre (elle atteint ce qu'il y a dessous) ; celle-ci garde les clics
			//    QUI LA VISENT et refuse seulement de VOLER LE FOCUS. Deux besoins
			//    voisins, deux styles differents.
			//
			// ⚠️ PORTEE REELLE, PAR PLATEFORME -- et ce qui n'est pas tenu est REFUSE A
			//    VOIX HAUTE au lieu d'etre ignore :
			//      Win32  : WS_EX_NOACTIVATE + SW_SHOWNOACTIVATE. Tenu.
			//      autres : NON TENU aujourd'hui. Le dorsal ecrit un refus nomme dans le
			//               journal au lieu de laisser croire que la fenetre est
			//               discrete -- un reglage affiche qui ne change rien est pire
			//               qu'un reglage absent.
			bool noActivate = false; ///< ne prend jamais le focus (fenetres de mesure)

			// --- Identité ---
			NkString title = "NkWindow";
			NkString name = "NkApp";
			NkString iconPath;
			NkNativeWindowOptions native;

			// --- Mobile / Safe Area ---
			bool respectSafeArea = true;

			// --- Android specifics ---
			bool hideSystemUI = false;	  // Masquer status bar + navigation bar
			bool lockOrientation = false; // Empêcher la rotation

			// --- Web / WASM ---
			NkWebInputOptions webInput;

			// ── Hints de surface ────────────────────────────────────────────────────
			// Remplis par NkGraphicsContextFactory::PrepareWindowConfig() AVANT Create().
			// NkWindow transmet ces hints au backend platform (XLib, XCB…)
			// sans les interpréter.
			// Pour la grande majorité des cas (Vulkan, Metal, DirectX, Software,
			// OpenGL/WGL, OpenGL/EGL) : laisser vide, ne rien faire.
			// Pour OpenGL/GLX sur Linux uniquement : appeler PrepareWindowConfig().
			NkSurfaceHints surfaceHints; // ← seul ajout dans ce struct
	};

} // namespace nkentseu
