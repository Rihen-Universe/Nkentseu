// =============================================================================
// NkImeTest — test minimal du clavier logiciel / IME.
//
// Ne dépend QUE de NKWindow + NKEvent (pas de NKCanvas/NKGui) : on affiche le
// clavier logiciel et on logge chaque caractère/touche reçu. Sur Android, la
// sortie va dans logcat (tag "NKIME") — vérifiable via :
//     adb logcat -s NKIME
//
// Interaction : toucher l'écran (ré)affiche le clavier ; taper -> logs.
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkTouchEvent.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)
#include <android/log.h>
#define IMELOG(...) __android_log_print(ANDROID_LOG_INFO, "NKIME", __VA_ARGS__)
#else
#include <cstdio>
#define IMELOG(...)                                                                                                    \
	do {                                                                                                               \
		printf("[NKIME] ");                                                                                            \
		printf(__VA_ARGS__);                                                                                           \
		printf("\n");                                                                                                  \
		fflush(stdout);                                                                                                \
	} while (0)
#endif

using namespace nkentseu;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkImeTest";
	d.appVersion = "0.1.0";
	return d;
})());

int nkmain(const NkEntryState &state) {
	(void)state;
	IMELOG("start");

	NkWindowConfig cfg;
	cfg.title = "IME Test";
	NkWindow window;
	if (!window.Create(cfg)) {
		IMELOG("window create FAILED");
		return -1;
	}
	IMELOG("window created id=%llu", (unsigned long long)window.GetId());

	auto &ev = NkEvents();

	ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { window.Close(); });

	// Le cœur du test : caractères Unicode remontés par l'IME.
	ev.AddEventCallback<NkTextInputEvent>(
		[&](NkTextInputEvent *e) { IMELOG("TEXT U+%04X utf8='%s'", (unsigned)e->GetCodepoint(), e->GetUtf8()); });

	// Touches spéciales (Backspace, Enter, Tab...).
	ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) { IMELOG("KEY %s", NkKeyToString(e->GetKey())); });

	// Toucher l'écran (ré)affiche le clavier.
	ev.AddEventCallback<NkTouchBeginEvent>([&](NkTouchBeginEvent *) {
		window.ShowSoftKeyboard(NkWindow::NkSoftKeyboardConfig{});
		IMELOG("touch -> ShowSoftKeyboard");
	});

	// Affiche le clavier dès le lancement.
	window.ShowSoftKeyboard(NkWindow::NkSoftKeyboardConfig{});
	IMELOG("keyboard requested (visible=%d)", (int)window.IsSoftKeyboardVisible());

	while (window.IsOpen()) {
		ev.PollEvents();
	}

	IMELOG("end");
	return 0;
}
