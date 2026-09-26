// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWin32EventSystem.cpp
// Implémentation Win32 des méthodes platform-spécifiques de NkEventSystem.
//
// Points appliqués :
//   Point 1 : NkWESystem::Events() remplace NkEventSystem::Instance()
//   Point 2 : NkWin32FindWindow() remplace gNkWin32WindowMap direct
//   Point 4 : WM_DESTROY conditionne PostQuitMessage au compte de fenêtres
//             Chaque event porte son winId via Enqueue(evt, winId)
//   Point 5 : Enqueue() (dans NkEventSystem) prend le winId — plus de
//             SetWindowId séparé ici, tout est atomique dans Enqueue
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeycodeMap.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Platform/Win32/NkWin32Window.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h> // DragAcceptFiles/DragQueryFile (drop OS)
#include <windowsx.h>
#include <dwmapi.h>
#include <vector>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib") // DragAcceptFiles/DragQueryFile

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

#include "NKWindow/Platform/Win32/NkWin32EventSystem.h"

namespace nkentseu {
	using namespace math;

	// =========================================================================
	// Donnees Win32 statiques -- remplacent les champs de NkEventSystemData
	// qui etaient accedes via this->mData dans les methodes membres.
	// =========================================================================
	namespace {
		struct NkWin32EvtData {
				bool mRawInputRegistered = false;
				int32 mPrevMouseX = 0;
				int32 mPrevMouseY = 0;
		};

		static NkWin32EvtData sWin32Data;
	} // namespace

	// Forward déclaration unique — NkWin32WndProc appelle NkWin32_ProcessMessage
	// qui est défini plus bas dans le fichier.
	static LRESULT NkWin32_ProcessMessage(NkEventSystem &sys, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
										  NkWindowId winId, NkWindow *owner);

	// =============================================================================
	// Init
	// =============================================================================

	bool NkEventSystem::Init() {
		if (mReady)
			return true;
		mTotalEventCount = 0;
		{
			NkScopedSpinLock lock(mQueueMutex);
			mEventQueue.Clear();
		}
		mPumping = false;
		mReady = true;
		return true;
	}

	// =============================================================================
	// PumpOS
	// =============================================================================

	void NkEventSystem::PumpOS() {
		if (mPumping)
			return;
		mPumping = true;

		MSG msg = {};
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessageW(&msg); // route vers NkWin32WndProc
		}

		mPumping = false;
	}

	// =============================================================================
	// GetPlatformName
	// =============================================================================

	const char *NkEventSystem::GetPlatformName() const noexcept {
		return "Win32";
	}

	void NkEventSystem::Enqueue_Public(NkEvent &evt, NkWindowId winId) {
		Enqueue(evt, winId);
	}

	// =============================================================================
	// NkWin32WndProc — procédure de fenêtre Win32
	//
	// Point 1 : NkWESystem::Events() remplace NkEventSystem::Instance()
	// Point 2 : NkWin32RegisterWindow / NkWin32FindWindow remplacent les globals
	// =============================================================================

	LRESULT CALLBACK NkWin32WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		// Point 1 : accès via NkSystem, pas via singleton de NkEventSystem
		auto &sys = NkWESystem::Events();

		if (msg == WM_NCCREATE) {
			auto *cs = reinterpret_cast<CREATESTRUCTW *>(lp);
			auto *win = static_cast<NkWindow *>(cs->lpCreateParams);
			if (win) {
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);

				// Raw input sur la première fenêtre
				if (!sWin32Data.mRawInputRegistered) {
					sWin32Data.mRawInputRegistered = true;
					RAWINPUTDEVICE rid{};
					rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
					rid.usUsage = HID_USAGE_GENERIC_MOUSE;
					rid.dwFlags = RIDEV_INPUTSINK;
					rid.hwndTarget = hwnd;
					RegisterRawInputDevices(&rid, 1, sizeof(rid));
				}

				// Point 2 : via fonction d'accès, pas extern
				NkWin32RegisterWindow(hwnd, win);
			}
			return DefWindowProcW(hwnd, msg, wp, lp);
		}

		if (msg == WM_NCDESTROY) {
			// Point 2 : via fonction d'accès
			NkWin32UnregisterWindow(hwnd);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
			return DefWindowProcW(hwnd, msg, wp, lp);
		}

		// O(1) lookup via GWLP_USERDATA
		auto *owner = reinterpret_cast<NkWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		NkWindowId winId = owner ? owner->GetId() : NK_INVALID_WINDOW_ID;

		// Fenetre SANS decoration (frame=false) : supprime la zone non-cliente
		// (barre de titre + bordure OS) en gardant le comportement (snap/min/max/
		// resize). On etend la zone client a toute la fenetre.
		if (msg == WM_NCCALCSIZE && wp == TRUE && owner && owner->mData.mBorderless) {
			NCCALCSIZE_PARAMS *p = reinterpret_cast<NCCALCSIZE_PARAMS *>(lp);
			if (IsZoomed(hwnd)) { // maximise : insere le cadre pour ne pas couvrir la barre des taches
				const int fx = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
				const int fy = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
				p->rgrc[0].left += fx;
				p->rgrc[0].top += fy;
				p->rgrc[0].right -= fx;
				p->rgrc[0].bottom -= fy;
			}
			return 0; // client = toute la fenetre (aucune bordure dessinee par l'OS)
		}

		return NkWin32_ProcessMessage(sys, hwnd, msg, wp, lp, winId, owner);
	}

	// =============================================================================
	// Helpers
	// =============================================================================

	static NkModifierState NkWin32_CurrentMods() {
		NkModifierState m;
		m.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		m.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
		m.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		m.super = ((GetKeyState(VK_LWIN) & 0x8000) != 0) || ((GetKeyState(VK_RWIN) & 0x8000) != 0);
		m.capLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
		m.numLock = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
		return m;
	}

	static NkKey NkWin32_VkeyToNkKey(UINT vk, bool extended) noexcept {
		return NkKeycodeMap::NkKeyFromWin32VK((uint32)vk, extended);
	}

	// =============================================================================
	// ProcessMessage
	//
	// Point 4 : WM_DESTROY — PostQuitMessage uniquement si c'est la dernière fenêtre.
	//           Chaque Enqueue(evt, winId) estampille l'event avec le bon winId.
	// Point 5 : Enqueue() (dans NkEventSystem.cpp) gère le lock de mQueueMutex
	//           de façon atomique — pas de double SetWindowId ici.
	// =============================================================================

	static LRESULT NkWin32_ProcessMessage(NkEventSystem &sys, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
										  NkWindowId winId, NkWindow *owner) {
		LRESULT result = 0;
		bool suppressDefaultProc = false;

		auto EnqueueForWindow = [&](NkEvent &evt) { sys.Enqueue_Public(evt, winId); };

		switch (msg) {
				// =====================================================================
				// Fenêtre — cycle de vie
				// =====================================================================

			case WM_CREATE: {
				DragAcceptFiles(hwnd, TRUE); // drop de fichiers OS -> NkDropFileEvent
				NkWindowCreateEvent evt(owner ? owner->GetConfig().width : 0u, owner ? owner->GetConfig().height : 0u,
										winId);
				EnqueueForWindow(evt);
				break;
			}

			case WM_DROPFILES: { // fichiers déposés depuis l'OS (Explorateur Windows…)
				HDROP drop = reinterpret_cast<HDROP>(wp);
				POINT pt{};
				DragQueryPoint(drop, &pt); // position CLIENT du dépôt
				NkDropFileData data;
				data.x = static_cast<int32>(pt.x);
				data.y = static_cast<int32>(pt.y);
				const UINT n = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
				for (UINT i = 0; i < n; ++i) {
					wchar_t wbuf[1024];
					if (DragQueryFileW(drop, i, wbuf, 1024) == 0)
						continue;
					char u8[2048]; // UTF-16 -> UTF-8 (chemins accentués corrects)
					const int len =
						WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, u8, static_cast<int>(sizeof(u8)), nullptr, nullptr);
					if (len > 0)
						data.AddPath(NkString(u8));
				}
				DragFinish(drop);
				if (data.Count() > 0) {
					NkDropFileEvent evt(data, winId);
					EnqueueForWindow(evt);
				}
				break;
			}

			case WM_CLOSE: {
				NkWindowCloseEvent evt(false, winId);
				EnqueueForWindow(evt);
				suppressDefaultProc = true;
				break;
			}

			case WM_DESTROY: {
				// Point 4 : l'event porte le winId de la fenêtre détruite.
				// PostQuitMessage seulement si c'est la dernière fenêtre enregistrée.
				NkWindowDestroyEvent evt(winId);
				EnqueueForWindow(evt);

				// On désenregistre la fenêtre avant de vérifier le compte
				// (UnregisterWindow est déjà appelé dans NkWindow::Close,
				//  mais WM_DESTROY peut arriver avant Close dans certains flux)
				uint32 remaining = NkWESystem::Instance().GetWindowCount();
				if (remaining == 0) {
					PostQuitMessage(0);
				}
				break;
			}

			case WM_PAINT: {
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
				NkWindowPaintEvent evt((int32)ps.rcPaint.left, (int32)ps.rcPaint.top,
									   (uint32)(ps.rcPaint.right - ps.rcPaint.left),
									   (uint32)(ps.rcPaint.bottom - ps.rcPaint.top), winId);
				EnqueueForWindow(evt);
				break;
			}

			// ── LE FOND EST PEINT — `bgColor` devient visible (26/09) ─────────────
			//
			// AVANT : on rendait 1 sans toucher au HDC. La brosse de la classe,
			// alimentee par `config.bgColor` depuis le 25/09, n'etait donc JAMAIS
			// employee pour remplir la fenetre : Windows demandait « efface le
			// fond », on repondait « c'est fait », et rien n'etait peint. Une
			// fenetre qui ne dessine pas gardait les pixels qui trainaient.
			// `bgColor` n'agissait que sur la zone decouverte PENDANT un
			// redimensionnement, peinte par le systeme avant WM_PAINT — donc
			// invisible des que `resizable` vaut false.
			//
			// Rodolf l'a constate le 26/09 en direct : « la bgColor n'est pas
			// appliquee ». Elle ne l'etait pas, en effet.
			//
			// ⚠️ POURQUOI CE N'EST PAS LE CLIGNOTEMENT QU'ON AVAIT CHASSE. Le
			//    clignotement vient d'un DESACCORD de couleur entre ce que le fond
			//    etale et ce que WM_PAINT dessine ensuite, pas du remplissage
			//    lui-meme. Ici on peint la couleur que l'appelant a DEMANDEE : s'il
			//    dessine par-dessus, il couvre sa propre couleur ; s'il ne dessine
			//    rien — le cas de tous les exemples d'enseignement — il obtient
			//    enfin le fond qu'il a decrit.
			//
			// ⚠️ ON LIT LA BROSSE DE LA CLASSE, on n'en fabrique pas une. Elle
			//    appartient a la classe et c'est `UnregisterClass` qui la detruit
			//    (piege paye le 25/09 : un cache de brosses rendait un HBRUSH mort
			//    a la deuxieme fenetre). `GetClassLongPtrW` ne transfere aucune
			//    propriete : il n'y a rien a liberer ici.
			//
			// ⚠️ ET SI LA BROSSE MANQUE, on ne peint pas et on rend 1 comme avant
			//    — jamais un fond de repli d'une autre couleur, qui ferait croire a
			//    un `bgColor` tenu alors qu'il ne l'est pas.
			// ⚠️ LA BROSSE DE LA FENETRE L'EMPORTE SUR CELLE DE LA CLASSE. Celle
			//    de la classe vient de `config.bgColor` a la creation et se
			//    partage entre toutes les fenetres de meme `config.name` ; celle
			//    de la fenetre est posee par `SetBackgroundColor` et n'appartient
			//    qu'a elle. Sans cette preference, deux fenetres de meme `name`
			//    ne pourraient toujours pas avoir deux fonds differents.
			case WM_ERASEBKGND: {
				HBRUSH brosse = reinterpret_cast<HBRUSH>(GetPropW(hwnd, kNkWin32PropFond));
				if (brosse == nullptr)
					brosse = reinterpret_cast<HBRUSH>(GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND));
				HDC hdc = reinterpret_cast<HDC>(wp);
				if (brosse != nullptr && hdc != nullptr) {
					RECT client = {};
					if (GetClientRect(hwnd, &client))
						FillRect(hdc, &client, brosse);
				}
				result = 1;
				break;
			}

			// =====================================================================
			// Fenêtre — focus / visibilité
			// =====================================================================
			case WM_ACTIVATE:
				if (wp == WA_ACTIVE) {
					NkWindowFocusGainedEvent evt(winId);
					EnqueueForWindow(evt);
				} else if (wp == WA_INACTIVE) {
					NkWindowFocusLostEvent evt(winId);
					EnqueueForWindow(evt);
				}
				break;

			case WM_SETFOCUS: {
				NkWindowFocusGainedEvent evt(winId);
				EnqueueForWindow(evt);
				break;
			}

			case WM_KILLFOCUS: {
				NkWindowFocusLostEvent evt(winId);
				EnqueueForWindow(evt);
				break;
			}

			case WM_SHOWWINDOW: {
				if (wp) {
					NkWindowShownEvent e(winId);
					EnqueueForWindow(e);
				} else {
					NkWindowHiddenEvent e(winId);
					EnqueueForWindow(e);
				}
				break;
			}

				// =====================================================================
				// Fenêtre — taille / position
				// =====================================================================

			case WM_SIZING: {
				RECT *r = reinterpret_cast<RECT *>(lp);
				if (r) {
					RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_NOERASE | RDW_INTERNALPAINT | RDW_UPDATENOW);

					NkWindowResizeEvent::ResizeState state;

					float32 newarea = owner->GetConfig().width * owner->GetConfig().height;
					float32 area = (uint32)(r->right - r->left) * (float32)(r->bottom - r->top);

					if (newarea > area) {
						state = NkWindowResizeEvent::ResizeState::NK_EXPANDED;
					} else if (newarea < area) {
						state = NkWindowResizeEvent::ResizeState::NK_REDUCED;
					}

					NkWindowResizeEvent evt((uint32)(r->right - r->left), (uint32)(r->bottom - r->top),
											owner ? owner->GetConfig().width : 0u,
											owner ? owner->GetConfig().height : 0u, winId, state);
					EnqueueForWindow(evt);
				}
				break;
			}

			case WM_SIZE: {
				uint32 nw = LOWORD(lp), nh = HIWORD(lp);
				// PAS DE REPEINT SYNCHRONE SUR UNE FENETRE REDUITE. RDW_UPDATENOW
				// force le rendu ICI, dans la procedure de fenetre : sur une
				// minimisation (taille nulle) cela reentre dans la boucle de rendu
				// au pire moment, pendant que la swapchain change de taille.
				if (nw > 0 && nh > 0)
					RedrawWindow(hwnd, NULL, NULL,
								 RDW_INVALIDATE | RDW_NOERASE | RDW_INTERNALPAINT | RDW_UPDATENOW);

				NkWindowResizeEvent::ResizeState state;

				float32 newarea = owner->GetConfig().width * owner->GetConfig().height;
				float32 area = nw * (float32)nh;

				if (newarea > area) {
					state = NkWindowResizeEvent::ResizeState::NK_EXPANDED;
				} else if (newarea < area) {
					state = NkWindowResizeEvent::ResizeState::NK_REDUCED;
				}

				NkWindowResizeEvent evt(nw, nh, owner ? owner->GetConfig().width : 0u,
										owner ? owner->GetConfig().height : 0u, winId, state);
				EnqueueForWindow(evt);
				if (wp == SIZE_MINIMIZED) {
					NkWindowMinimizeEvent e;
					EnqueueForWindow(e);
				} else if (wp == SIZE_MAXIMIZED) {
					NkWindowMaximizeEvent e;
					EnqueueForWindow(e);
				} else if (wp == SIZE_RESTORED) {
					NkWindowRestoreEvent e;
					EnqueueForWindow(e);
				}
				break;
			}

			case WM_ENTERSIZEMOVE: {
				// Le drag move/resize entre dans une boucle modale qui bloque le thread
				// (DispatchMessageW ne rend pas la main) -> la boucle de rendu gele. On
				// arme un timer : son WM_TIMER, fire DANS la boucle modale, declenche le
				// callback "rendre une frame" enregistre par l'appli. Cf. WM_TIMER plus bas.
				SetTimer(hwnd, /*NK_SIZEMOVE_TIMER*/ 0xB1A5u, 8 /*ms ~125fps*/, nullptr);
				NkWindowResizeBeginEvent beginResize(winId);
				EnqueueForWindow(beginResize);
				NkWindowMoveBeginEvent beginMove(winId);
				EnqueueForWindow(beginMove);
				break;
			}

			case WM_TIMER: {
				// Frame rendue pendant la boucle modale (drag move/resize) -> pas de gel.
				if (wp == 0xB1A5u) {
					sys.InvokeSizeMoveFrame();
					return 0;
				}
				break;
			}

			case WM_EXITSIZEMOVE: {
				KillTimer(hwnd, 0xB1A5u);
				NkWindowResizeEndEvent endResize(winId);
				EnqueueForWindow(endResize);
				NkWindowMoveEndEvent endMove(winId);
				EnqueueForWindow(endMove);
				break;
			}

			case WM_MOVE: {
				NkWindowMoveEvent evt((int32)LOWORD(lp), (int32)HIWORD(lp), 0, 0, winId);
				EnqueueForWindow(evt);
				break;
			}

			case WM_DPICHANGED: {
				WORD dpi = HIWORD(wp);
				NkWindowDpiEvent evt((float32)dpi / USER_DEFAULT_SCREEN_DPI, owner ? owner->GetDpiScale() : 1.f,
									 (uint32)dpi, winId);
				EnqueueForWindow(evt);
				const RECT *r = reinterpret_cast<const RECT *>(lp);
				if (r)
					SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
								 SWP_NOZORDER | SWP_NOACTIVATE);
				break;
			}

			case WM_DISPLAYCHANGE: {
				// Broadcast OS : un écran a changé de résolution/profondeur, ou
				// un moniteur a été branché/débranché. WM_DISPLAYCHANGE ne dit
				// pas lequel → on compare le nombre de moniteurs au compte
				// précédent (cache global) pour déduire ADDED / REMOVED, sinon
				// RESOLUTION_CHANGED. Pour ADDED/REMOVED, l'app doit ré-énumérer
				// via NkWindow::EnumerateMonitors() pour obtenir le détail.
				static int sPrevMonitorCount = -1;
				const int curCount = GetSystemMetrics(SM_CMONITORS);
				NkDisplayChange change;
				if (sPrevMonitorCount < 0 || curCount == sPrevMonitorCount)
					change = NkDisplayChange::NK_DISPLAY_RESOLUTION_CHANGED;
				else if (curCount > sPrevMonitorCount)
					change = NkDisplayChange::NK_DISPLAY_ADDED;
				else
					change = NkDisplayChange::NK_DISPLAY_REMOVED;
				sPrevMonitorCount = curCount;

				NkDisplayInfo info = owner ? owner->GetCurrentMonitor() : NkDisplayInfo{};
				NkSystemDisplayEvent evt(change, info, winId);
				EnqueueForWindow(evt);
				break;
			}

				// =====================================================================
				// Souris
				// =====================================================================

			case WM_MOUSEMOVE: {
				if (owner && !owner->mData.mMouseTracking) {
					TRACKMOUSEEVENT tme{};
					tme.cbSize = sizeof(tme);
					tme.dwFlags = TME_LEAVE;
					tme.hwndTrack = hwnd;
					if (TrackMouseEvent(&tme)) {
						owner->mData.mMouseTracking = true;
					}
					NkMouseEnterEvent enterEvt(winId);
					EnqueueForWindow(enterEvt);
				}

				int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
				POINT pt = {x, y};
				ClientToScreen(hwnd, &pt);
				NkMouseButtons buttons;
				SHORT mk = LOWORD(wp);
				if (mk & MK_LBUTTON)
					buttons.Set(NkMouseButton::NK_MB_LEFT);
				if (mk & MK_RBUTTON)
					buttons.Set(NkMouseButton::NK_MB_RIGHT);
				if (mk & MK_MBUTTON)
					buttons.Set(NkMouseButton::NK_MB_MIDDLE);
				if (mk & MK_XBUTTON1)
					buttons.Set(NkMouseButton::NK_MB_BACK);
				if (mk & MK_XBUTTON2)
					buttons.Set(NkMouseButton::NK_MB_FORWARD);
				NkMouseMoveEvent evt(x, y, (int32)pt.x, (int32)pt.y, x - sWin32Data.mPrevMouseX,
									 y - sWin32Data.mPrevMouseY, buttons, NkWin32_CurrentMods(), winId);
				sWin32Data.mPrevMouseX = x;
				sWin32Data.mPrevMouseY = y;
				EnqueueForWindow(evt);
				break;
			}

			case WM_INPUT: {
				UINT sz = 0;
				GetRawInputData((HRAWINPUT)lp, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER));
				if (sz > 0) {
					NkVector<BYTE> buf;
					buf.Resize(static_cast<usize>(sz));
					if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, buf.Data(), &sz, sizeof(RAWINPUTHEADER)) == sz) {
						auto *raw = reinterpret_cast<const RAWINPUT *>(buf.Data());
						if (raw->header.dwType == RIM_TYPEMOUSE) {
							NkMouseRawEvent evt(raw->data.mouse.lLastX, raw->data.mouse.lLastY, 0, winId);
							EnqueueForWindow(evt);
						}
					}
				}
				break;
			}

			case WM_MOUSEWHEEL: {
				POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				ScreenToClient(hwnd, &pt);
				SHORT mk = LOWORD(wp);
				double delta = GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA;
				NkModifierState mods;
				mods.ctrl = !!(mk & MK_CONTROL);
				mods.shift = !!(mk & MK_SHIFT);
				NkMouseWheelVerticalEvent evt(delta, (int32)pt.x, (int32)pt.y, 0.0, false, mods, winId);
				EnqueueForWindow(evt);
				break;
			}

			case WM_MOUSEHWHEEL: {
				POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				ScreenToClient(hwnd, &pt);
				double delta = GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA;
				NkMouseWheelHorizontalEvent evt(delta, (int32)pt.x, (int32)pt.y, winId);
				EnqueueForWindow(evt);
				break;
			}

#define NK_MB_PRESS(Button)                                                                                            \
	{                                                                                                                  \
		POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};                                                               \
		POINT sp = pt;                                                                                                 \
		ClientToScreen(hwnd, &sp);                                                                                     \
		NkMouseButtonPressEvent evt(NkMouseButton::Button, pt.x, pt.y, sp.x, sp.y, 1, NkWin32_CurrentMods(), winId);   \
		EnqueueForWindow(evt);                                                                                         \
	}                                                                                                                  \
	break

#define NK_MB_RELEASE(Button)                                                                                          \
	{                                                                                                                  \
		POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};                                                               \
		POINT sp = pt;                                                                                                 \
		ClientToScreen(hwnd, &sp);                                                                                     \
		NkMouseButtonReleaseEvent evt(NkMouseButton::Button, pt.x, pt.y, sp.x, sp.y, 1, NkWin32_CurrentMods(), winId); \
		EnqueueForWindow(evt);                                                                                         \
	}                                                                                                                  \
	break

			case WM_LBUTTONDOWN:
				NK_MB_PRESS(NK_MB_LEFT);
			case WM_LBUTTONUP:
				NK_MB_RELEASE(NK_MB_LEFT);
			case WM_RBUTTONDOWN:
				NK_MB_PRESS(NK_MB_RIGHT);
			case WM_RBUTTONUP:
				NK_MB_RELEASE(NK_MB_RIGHT);
			case WM_MBUTTONDOWN:
				NK_MB_PRESS(NK_MB_MIDDLE);
			case WM_MBUTTONUP:
				NK_MB_RELEASE(NK_MB_MIDDLE);

#undef NK_MB_PRESS
#undef NK_MB_RELEASE

			// =================================================================
			//  ⚠️ UN DOUBLE-CLIC EST AUSSI UN APPUI, ET WINDOWS NOUS L'AVAIT PRIS
			// =================================================================
			//  La classe de fenetre porte `CS_DBLCLKS` (NkWin32Window.cpp:487).
			//  Windows REMPLACE alors le second `WM_xBUTTONDOWN` par
			//  `WM_xBUTTONDBLCLK` -- il ne l'ajoute pas. Ce bloc n'emettait que
			//  `NkMouseDoubleClickEvent` : le second appui n'existait donc POUR
			//  PERSONNE. Consequences mesurees le 2026-09-01 :
			//    - `NkGuiInput::mouseDown[b]` reste FAUX pendant le second clic,
			//      donc `mouseClicked[b]` aussi ;
			//    - tout consommateur qui croise le double-clic avec un appui
			//      (`if (mousePressed) { ... if (doubleClick) ... }`) ne voit
			//      RIEN -- c'est le defaut « double-cliquer ne donne pas acces a
			//      la modification fine » rapporte par Rodolf sur NkUIDesign ;
			//    - la detection INTERNE de secours de NKGui (2 clics < 0,40 s)
			//      ne peut pas prendre le relais : elle s'arme sur
			//      `mouseClicked`, qui n'arrive jamais ;
			//    - un GLISSER amorce sur un double-clic est impossible, le
			//      bouton etant declare relache alors qu'il est enfonce.
			//
			//  ⚠️ ET LE CONTRAT ETAIT DEJA ECRIT, il n'etait pas honore.
			//     `NkMouseEvent.h:543-545` dit mot pour mot : *« GetClickCount()
			//     permet de distinguer un simple-clic manuel d'un double-clic
			//     detecte par l'OS (ce dernier genere AUSSI
			//     NkMouseDoubleClickEvent) »* -- « aussi » suppose l'appui. On
			//     ne change donc pas le contrat : on le rend vrai. L'appui porte
			//     `clickCount = 2`, ce qui permet a qui le veut de distinguer.
			//
			//  L'ordre compte : l'APPUI d'abord, le double-clic ensuite -- un
			//  consommateur qui lit les deux dans l'ordre d'arrivee voit le
			//  geste comme il s'est produit.
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDBLCLK: {
				POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				POINT sp = pt;
				ClientToScreen(hwnd, &sp);
				NkMouseButton btn = (msg == WM_LBUTTONDBLCLK)	? NkMouseButton::NK_MB_LEFT
									: (msg == WM_RBUTTONDBLCLK) ? NkMouseButton::NK_MB_RIGHT
																: NkMouseButton::NK_MB_MIDDLE;
				NkModifierState mods = NkWin32_CurrentMods();
				NkMouseButtonPressEvent press(btn, pt.x, pt.y, sp.x, sp.y, 2, mods, winId);
				EnqueueForWindow(press);
				NkMouseDoubleClickEvent evt(btn, pt.x, pt.y, sp.x, sp.y, mods, winId);
				EnqueueForWindow(evt);
				break;
			}

			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP: {
				POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
				POINT sp = pt;
				ClientToScreen(hwnd, &sp);
				NkMouseButton btn = (HIWORD(wp) & XBUTTON1) ? NkMouseButton::NK_MB_BACK : NkMouseButton::NK_MB_FORWARD;
				if (msg == WM_XBUTTONDOWN) {
					NkMouseButtonPressEvent e(btn, pt.x, pt.y, sp.x, sp.y, 1, NkWin32_CurrentMods(), winId);
					EnqueueForWindow(e);
				} else {
					NkMouseButtonReleaseEvent e(btn, pt.x, pt.y, sp.x, sp.y, 1, NkWin32_CurrentMods(), winId);
					EnqueueForWindow(e);
				}
				break;
			}

			case WM_MOUSELEAVE: {
				if (owner) {
					owner->mData.mMouseTracking = false;
				}
				NkMouseLeaveEvent evt(winId);
				EnqueueForWindow(evt);
				break;
			}

				// =====================================================================
				// Clavier
				// =====================================================================

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP: {
				UINT sc = (lp >> 16) & 0xFF;
				bool isExt = (lp >> 24) & 1;
				bool isRep = (lp >> 30) & 1;
				NkScancode nkSc = NkScancodeFromWin32(sc, isExt);
				NkKey k = NkScancodeToKey(nkSc);
				if (k == NkKey::NK_UNKNOWN)
					k = NkWin32_VkeyToNkKey((UINT)wp, isExt);
				if (k != NkKey::NK_UNKNOWN) {
					bool isPress = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
					NkModifierState mods = NkWin32_CurrentMods();
					UINT nativeKey = (UINT)wp;
					if (isPress && isRep) {
						NkKeyRepeatEvent e(k, 1, nkSc, mods, nativeKey, isExt, winId);
						EnqueueForWindow(e);
					} else if (isPress) {
						NkKeyPressEvent e(k, nkSc, mods, nativeKey, isExt, winId);
						EnqueueForWindow(e);
					} else {
						NkKeyReleaseEvent e(k, nkSc, mods, nativeKey, isExt, winId);
						EnqueueForWindow(e);
					}
				}
				break;
			}

			case WM_CHAR: {
				UINT cp = (UINT)wp;
				if (cp >= 32 && cp != 127) {
					NkTextInputEvent evt(cp, winId);
					EnqueueForWindow(evt);
				}
				break;
			}

				// =====================================================================
				// Hit-test (fenêtre sans bordure)
				// =====================================================================

			case WM_NCHITTEST:
				if (owner && !owner->GetConfig().frame) {
					// TOUT = HTCLIENT : la zone client couvre toute la fenetre (NCCALCSIZE=0),
					// donc l'app recoit WM_MOUSEMOVE jusqu'aux bords -> elle detecte le bord
					// survole (coords client) et delegue a l'OS via NkWindow::BeginResize
					// (hand-off natif WM_NCLBUTTONDOWN). Approche CROSS-PLATFORM (chaque
					// backend implemente BeginResize nativement). Le drag de titre = idem
					// via BeginDragMove. Voir NkEditorShell::HandleEdgeResize.
					result = HTCLIENT;
				}
				break;

			// =====================================================================
			// Bornes de taille — EN COORDONNEES FENETRE
			//
			// ⚠️ LE DEFAUT CORRIGE LE 25/09 : `ptMinTrackSize` etait rempli avec les
			//    valeurs CLIENT telles quelles. Or MINMAXINFO parle en coordonnees
			//    FENETRE, bordure et barre de titre comprises. Demander un client
			//    minimum de 160x90 laissait donc Windows descendre jusqu'a un client
			//    de 160-16 par 90-39 : PLUS PETIT QUE DEMANDE, et personne ne le
			//    disait. Oublier AdjustWindowRect, c'est livrer moins que promis.
			//
			// ⚠️ `ptMaxTrackSize` n'etait PAS touche : `maxWidth`/`maxHeight` etaient
			//    acceptes et jetes. 0xFFFF (le defaut) signifie « pas de plafond » et
			//    laisse donc Windows decider, comme avant.
			//
			// Fenetre sans cadre : WM_NCCALCSIZE rend toute la fenetre cliente, donc
			// fenetre == client et il ne faut RIEN ajuster (meme regle qu'a la
			// creation — l'ajustement y fabriquait deja une bande morte).
			// =====================================================================
			case WM_GETMINMAXINFO:
				if (owner) {
					auto *mm = reinterpret_cast<MINMAXINFO *>(lp);
					const NkWindowConfig &wc = owner->GetConfig();
					const bool borderless = owner->mData.mBorderless;
					// Le style VIVANT, pas celui memorise : SetDecorated a pu le changer.
					const DWORD st = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
					const DWORD ex = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

					RECT rcMin = {0, 0, (LONG)wc.minWidth, (LONG)wc.minHeight};
					if (!borderless)
						AdjustWindowRectEx(&rcMin, st, FALSE, ex);
					mm->ptMinTrackSize.x = rcMin.right - rcMin.left;
					mm->ptMinTrackSize.y = rcMin.bottom - rcMin.top;

					const bool plafonne = (wc.maxWidth > 0 && wc.maxWidth < 0xFFFF) ||
										  (wc.maxHeight > 0 && wc.maxHeight < 0xFFFF);
					if (plafonne) {
						const LONG cw = (wc.maxWidth > 0 && wc.maxWidth < 0xFFFF) ? (LONG)wc.maxWidth : 0xFFFF;
						const LONG ch = (wc.maxHeight > 0 && wc.maxHeight < 0xFFFF) ? (LONG)wc.maxHeight : 0xFFFF;
						RECT rcMax = {0, 0, cw, ch};
						if (!borderless)
							AdjustWindowRectEx(&rcMax, st, FALSE, ex);
						mm->ptMaxTrackSize.x = rcMax.right - rcMax.left;
						mm->ptMaxTrackSize.y = rcMax.bottom - rcMax.top;
					}
				}
				break;

			// =====================================================================
			// `closable` et `movable` : le style ne sait pas les dire
			//
			// Griser le menu systeme (NkWin32AppliquerMenuSysteme, cote Window)
			// eteint le bouton X et les entrees du menu, mais NI Alt+F4, NI le
			// double-clic sur la barre de titre, NI le glisser de titre ne passent
			// par ce menu : ils envoient directement le WM_SYSCOMMAND. Les deux
			// moities sont necessaires ; aucune ne tient seule.
			//
			// ⚠️ SC_MINIMIZE / SC_MAXIMIZE sont filtres pour l'UTILISATEUR seulement.
			//    `NkWindow::Minimize()` et `Maximize()` appellent ShowWindow
			//    directement et restent obeis : ce sont des ordres du PROGRAMME, et
			//    `minimizable = false` decrit ce que l'utilisateur peut faire de la
			//    fenetre, pas ce que le programme s'interdit.
			// =====================================================================
			// =====================================================================
			// `canFullscreen = false` : le plein ecran par l'UTILISATEUR est refuse
			//
			// Windows n'a pas de « mode plein ecran » que l'on puisse interdire par
			// un style. Ce que l'utilisateur a sous la main, c'est :
			//   • le raccourci systeme Win+Haut / Win+Fleche, qui arrive en
			//     WM_SYSCOMMAND SC_MAXIMIZE — deja filtre au-dessus quand
			//     `maximizable` ou `resizable` est faux, et filtre ici quand c'est
			//     `canFullscreen` qui est faux ;
			//   • la touche F11, que BEAUCOUP d'applications interpretent elles-memes.
			//     NKWindow ne lui donne AUCUN sens : elle part a l'application comme
			//     n'importe quelle touche. Interdire F11 ici serait interdire une
			//     touche, pas un mode — et casser les applications qui s'en servent
			//     pour autre chose.
			//
			// ⚠️ CE QUE CE REGLAGE NE FAIT PAS, et c'est dit plutot que laisse croire :
			//    il n'empeche pas `NkWindow::SetFullscreen(true)` — un appel explicite
			//    du programme est un ordre, exactement comme `Maximize()`. Et il
			//    n'empeche pas l'utilisateur de glisser la fenetre en haut de l'ecran
			//    (l'accrochage Aero), qui maximise sans passer par SC_MAXIMIZE : pour
			//    cela, c'est `resizable = false` qu'il faut, et il retire WS_THICKFRAME.
			// =====================================================================
			case WM_SYSCOMMAND: {
				if (owner) {
					const NkWindowConfig &wc = owner->GetConfig();
					const UINT cmd = (UINT)(wp & 0xFFF0);

					const bool interdit = (cmd == SC_CLOSE && !wc.closable) ||
										  (cmd == SC_MOVE && !wc.movable) ||
										  (cmd == SC_SIZE && !wc.resizable) ||
										  (cmd == SC_MINIMIZE && !wc.minimizable) ||
										  (cmd == SC_MAXIMIZE && (!wc.maximizable || !wc.resizable)) ||
										  (cmd == SC_MAXIMIZE && !wc.canFullscreen);
					if (interdit)
						suppressDefaultProc = true;
				}
				break;
			}

			// Glisser la barre de titre : DefWindowProc transforme ce message en
			// SC_MOVE, donc le filtre ci-dessus suffirait — mais l'arreter ici evite
			// en plus le clignotement du menu systeme sous le curseur.
			case WM_NCLBUTTONDOWN:
				if (owner && !owner->GetConfig().movable && wp == HTCAPTION)
					suppressDefaultProc = true;
				break;

			// =====================================================================
			// Curseur (zone client) — applique le curseur demandé par l'UI. Sans
			// ça, Windows réinitialise au curseur de classe (flèche) à chaque move.
			// =====================================================================
			case WM_SETCURSOR:
				if (owner && LOWORD(lp) == HTCLIENT && owner->mData.mClientCursor) {
					::SetCursor(owner->mData.mClientCursor);
					result = TRUE; // on a géré → empêche le reset par DefWindowProc
				}
				break;

			default:
				break;
		}

		if (suppressDefaultProc)
			return 0;
		if (result)
			return result;

		if (owner && owner->mData.mExternal && owner->mData.mPrevWndProc &&
			owner->mData.mPrevWndProc != NkWin32WndProc) {
			return CallWindowProcW(owner->mData.mPrevWndProc, hwnd, msg, wp, lp);
		}

		return DefWindowProcW(hwnd, msg, wp, lp);
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
