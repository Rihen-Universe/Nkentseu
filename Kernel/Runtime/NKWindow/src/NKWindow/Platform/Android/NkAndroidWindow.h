#pragma once
// =============================================================================
// NkAndroidWindow.h — Android platform data for NkWindow (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKEvent/NkSafeArea.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowId.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include <android/native_window.h>
#include <vector>
#include "NKContainers/Sequential/NkVector.h"

struct AConfiguration;
struct android_app;

namespace nkentseu {

	class NkAndroidDropTarget;

	struct NkWindowData {
			ANativeWindow *mNativeWindow = nullptr;
			AConfiguration *mAConfig = nullptr;
			struct android_app *mAndroidApp = nullptr;
			NkAndroidDropTarget *mDropTarget = nullptr;
			NkSurfaceHints mAppliedHints{};
			bool mExternal = false;

			uint32 mWidth = 0;
			uint32 mHeight = 0;
			uint32 mPrevWidth = 0;
			uint32 mPrevHeight = 0;

			bool mFullscreen = true;

			NkSafeAreaInsets mSafeArea{};
			NkScreenOrientation mOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
	};

	class NkWindow;
	NkWindow *NkAndroidFindWindowById(NkWindowId id);
	NkVector<NkWindow *> NkAndroidGetWindowsSnapshot();
	NkWindow *NkAndroidGetLastWindow();
	void NkAndroidRegisterWindow(NkWindow *window);
	void NkAndroidUnregisterWindow(NkWindow *window);
	bool NkAndroidHideSystemUI(struct android_app *app); // Masquer status bar + navigation bar
	// La bascule de volume regle le flux MEDIA meme quand l'application se tait
	// (Activity.setVolumeControlStream par JNI -- aucun Java a nous n'est requis).
	bool NkAndroidSetVolumeControlStream(struct android_app *app);

	// ── CE QUE LE SYSTEME REFUSE DE LIVRER, IL ACCEPTE DE LE FAIRE ──────
	// Android ne livre jamais les touches ACCUEIL, RECENTES, POWER, VEILLE.
	// Ces trois-la obtiennent le resultat par une autre porte, sans Java.
	/// Empeche l'ecran de s'eteindre tant que `actif` vaut vrai.
	bool NkAndroidGarderEcranAllume(struct android_app *app, bool actif);
	/// Epingle l'ecran : accueil et recentes cessent de sortir de l'application.
	/// Hors mode proprietaire d'appareil, le systeme laisse une sortie -- et
	/// c'est deliberé de sa part.
	bool NkAndroidEpinglerEcran(struct android_app *app, bool actif);
	/// Vrai si l'ecran est allume. `hors_service` rend vrai quand la question
	/// n'a pas pu etre posee -- une reponse par defaut qui se tait serait
	/// indiscernable d'un ecran allume.
	bool NkAndroidEcranAllume(struct android_app *app, bool *hors_service);

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_ANDROID
