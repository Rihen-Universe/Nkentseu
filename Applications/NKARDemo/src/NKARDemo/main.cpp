// =============================================================================
// NKARDemo — Étage 3 : la réalité augmentée, de la caméra à l'objet ancré.
//
// Ce que cette démo prouve (et rien de plus) :
//   - la caméra du poste alimente la chaîne AR de NKXR (NKCamera → NkArSession) ;
//   - un marqueur imprimé est détecté, identifié et SUIVI dans l'image ;
//   - un objet 3D est ancré sur lui : il tient sa place quand la feuille bouge,
//     et il est rendu par NKRenderer par-dessus la vidéo plein écran.
//
// TOUT SE RÈGLE PAR LE CODE (principe moteur n°4, docs/IDEES_ARCHITECTURE.md) :
// la structure NkArSessionConfig porte les réglages, et l'environnement n'est
// qu'une couche facultative appliquée explicitement plus bas.
//
// Sans caméra branchée, la démo ne ment pas : elle le DIT et bascule sur une
// image de synthèse animée — on peut donc la lancer partout, et la chaîne
// reste exercée de bout en bout.
//
// Imprimer le marqueur : la démo écrit « nkar_marqueur.png » au démarrage.
// L'imprimer sur A4, mesurer le côté du carré noir, et le donner dans
// NkArSessionConfig::markerSizeMeters — c'est lui qui donne l'ÉCHELLE.
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKImage/NkImage.h"

#include "NKCamera/NkCameraSystem.h"

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
	#include "NKFileSystem/NkFile.h"
	#include <android_native_app_glue.h>
	#include <jni.h>
namespace nkentseu {
	extern android_app *nk_android_global_app;
}
#endif

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

#include "NKXR/AR/NkArSession.h"
#include "NKXR/AR/NkArWorld.h"

#include <cstdlib>
#include <cstdio>

#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;
namespace nkxr = nkentseu::xr;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "NKARDemo";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {

	constexpr uint32 kCamWidth = 1280;
	constexpr uint32 kCamHeight = 720;
	constexpr int32 kMarkerId = 0x2D;

	// Image de repli quand aucune caméra n'est branchée : le marqueur y
	// dérive lentement, ce qui exerce le SUIVI (et pas seulement la détection).
	void SynthesizeFallback(uint8 *rgba, uint32 w, uint32 h, const uint8 *pattern, uint32 patternSize,
							float32 time) {
		for (uint32 i = 0; i < w * h; ++i) {
			rgba[i * 4u + 0u] = 190;
			rgba[i * 4u + 1u] = 195;
			rgba[i * 4u + 2u] = 205;
			rgba[i * 4u + 3u] = 255;
		}
		const float32 size = 260.f + 60.f * math::NkSin(time * 0.7f);
		const float32 cx = float32(w) * 0.5f + 180.f * math::NkSin(time * 0.5f);
		const float32 cy = float32(h) * 0.5f + 90.f * math::NkCos(time * 0.35f);
		const int32 x0 = int32(cx - size * 0.5f);
		const int32 y0 = int32(cy - size * 0.5f);
		const int32 side = int32(size);
		for (int32 y = 0; y < side; ++y) {
			for (int32 x = 0; x < side; ++x) {
				const int32 px = x0 + x;
				const int32 py = y0 + y;
				if (px < 0 || py < 0 || uint32(px) >= w || uint32(py) >= h) {
					continue;
				}
				const uint32 tu = uint32((float32(x) / float32(side)) * float32(patternSize - 1u));
				const uint32 tv = uint32((float32(y) / float32(side)) * float32(patternSize - 1u));
				const uint8 value = pattern[tv * patternSize + tu];
				uint8 *dst = &rgba[(uint32(py) * w + uint32(px)) * 4u];
				dst[0] = value;
				dst[1] = value;
				dst[2] = value;
				dst[3] = 255;
			}
		}
	}

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
	// Demande la permission CAMÉRA À L'EXÉCUTION.
	//
	// Pourquoi ce code existe : depuis Android 6, déclarer la permission dans le
	// manifeste — ce que fait déjà le descripteur jenga — ne fait que la rendre
	// DEMANDABLE. Tant que l'utilisateur ne l'a pas accordée, la caméra reste
	// muette, et une application native ne pose pas la question toute seule.
	// Mesuré au premier lancement sur le téléphone de Rihen : « granted=false »,
	// écran noir, et rien dans le journal pour l'expliquer.
	//
	// Une NativeActivity n'a pas de code Java à elle, mais elle a une machine
	// virtuelle : on appelle donc Activity.requestPermissions par JNI. Le
	// résultat arrive de façon asynchrone dans une fenêtre système ; l'appelant
	// doit donc attendre, et c'est à lui de ne pas figer sa boucle en attendant.
	bool NkAndroidRequestCameraPermission() {
		android_app *app = nkentseu::nk_android_global_app;
		if (app == nullptr || app->activity == nullptr || app->activity->vm == nullptr) {
			return false;
		}
		JNIEnv *env = nullptr;
		if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || env == nullptr) {
			return false;
		}
		bool granted = false;
		jclass activityClass = env->GetObjectClass(app->activity->clazz);
		jstring permission = env->NewStringUTF("android.permission.CAMERA");

		// checkSelfPermission : 0 = PERMISSION_GRANTED. On ne redemande pas ce
		// qui est déjà accordé — une fenêtre inutile à chaque lancement serait
		// une nuisance, et l'utilisateur finirait par refuser par réflexe.
		jmethodID checkSelf = env->GetMethodID(activityClass, "checkSelfPermission", "(Ljava/lang/String;)I");
		if (checkSelf != nullptr) {
			const jint result = env->CallIntMethod(app->activity->clazz, checkSelf, permission);
			granted = (result == 0);
		}
		if (!granted) {
			jmethodID requestPermissions =
				env->GetMethodID(activityClass, "requestPermissions", "([Ljava/lang/String;I)V");
			if (requestPermissions != nullptr) {
				jobjectArray array =
					env->NewObjectArray(1, env->FindClass("java/lang/String"), env->NewStringUTF(""));
				env->SetObjectArrayElement(array, 0, permission);
				env->CallVoidMethod(app->activity->clazz, requestPermissions, array, 1);
				env->DeleteLocalRef(array);
			}
		}
		env->DeleteLocalRef(permission);
		env->DeleteLocalRef(activityClass);
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
		app->activity->vm->DetachCurrentThread();
		return granted;
	}
#endif

	// Projette un point du repère CAMÉRA (avant = -Z) vers les pixels de
	// l'image, puis vers la zone d'affichage de la vidéo à l'écran. C'est la
	// projection de la VRAIE caméra, pas celle du renderer : l'augmentation se
	// superpose donc exactement à ce que l'objectif a vu.
	bool ProjectToScreen(const math::NkVec3f &p, const nkxr::NkArCameraIntrinsics &k, uint32 imgW, uint32 imgH,
						 const NkRectF &dst, NkVec2f &out) {
		const float32 depth = -p.z;
		if (depth <= 0.01f) {
			return false; // derrière la caméra : rien à dessiner
		}
		const float32 u = k.fx * (p.x / depth) + k.cx;
		const float32 v = -k.fy * (p.y / depth) + k.cy;
		out.x = dst.x + (u / float32(imgW)) * dst.width;
		out.y = dst.y + (v / float32(imgH)) * dst.height;
		return true;
	}

	// Découpe un segment de l'écran sur le rectangle de la vidéo (Liang-Barsky).
	// Sans cela, un objet qui sort du champ serait peint sur les bandes noires,
	// ou disparaîtrait d'un bloc. Or ce qui sort du champ doit être COUPÉ : un
	// objet réel ne s'évanouit pas parce qu'il touche le bord de l'image.
	bool ClipToRect(const NkRectF &r, NkVec2f &a, NkVec2f &b) {
		float32 t0 = 0.f;
		float32 t1 = 1.f;
		const float32 dx = b.x - a.x;
		const float32 dy = b.y - a.y;
		const float32 p[4] = { -dx, dx, -dy, dy };
		const float32 q[4] = { a.x - r.x, (r.x + r.width) - a.x, a.y - r.y, (r.y + r.height) - a.y };
		for (uint32 i = 0; i < 4u; ++i) {
			if (p[i] > -1e-6f && p[i] < 1e-6f) {
				if (q[i] < 0.f) {
					return false; // parallèle au bord et hors du rectangle
				}
				continue;
			}
			const float32 t = q[i] / p[i];
			if (p[i] < 0.f) {
				if (t > t1) {
					return false;
				}
				if (t > t0) {
					t0 = t;
				}
			}
			else {
				if (t < t0) {
					return false;
				}
				if (t < t1) {
					t1 = t;
				}
			}
		}
		const NkVec2f start{ a.x + dx * t0, a.y + dy * t0 };
		const NkVec2f end{ a.x + dx * t1, a.y + dy * t1 };
		a = start;
		b = end;
		return true;
	}

	// Dessine une arête donnée dans le repère CAMÉRA, en la coupant deux fois :
	// d'abord sur le plan rapproché (la partie derrière l'objectif n'existe pas
	// et sa projection est absurde), ensuite sur le bord de l'image. C'est ce
	// double découpage qui rend la disparition PROGRESSIVE au lieu de brutale.
	void DrawEdge3D(NkRender2D *r2d, const math::NkVec3f &a, const math::NkVec3f &b,
					const nkxr::NkArCameraIntrinsics &k, uint32 imgW, uint32 imgH, const NkRectF &dst,
					const NkVec4f &color, float32 thickness) {
		const float32 kNear = 0.02f;
		math::NkVec3f p0 = a;
		math::NkVec3f p1 = b;
		const float32 d0 = -p0.z;
		const float32 d1 = -p1.z;
		if (d0 <= kNear && d1 <= kNear) {
			return; // entièrement derrière : rien à montrer
		}
		if (d0 <= kNear || d1 <= kNear) {
			// Un seul bout derrière : on coupe pile sur le plan rapproché.
			const float32 t = (kNear - d0) / (d1 - d0);
			const math::NkVec3f cut = p0 + (p1 - p0) * t;
			if (d0 <= kNear) {
				p0 = cut;
			}
			else {
				p1 = cut;
			}
		}
		NkVec2f s0, s1;
		if (!ProjectToScreen(p0, k, imgW, imgH, dst, s0) || !ProjectToScreen(p1, k, imgW, imgH, dst, s1)) {
			return;
		}
		if (!ClipToRect(dst, s0, s1)) {
			return;
		}
		r2d->DrawLine(s0, s1, color, thickness);
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
	// ── AVANT TOUT LE RESTE ───────────────────────────────────────────────────
	// Sur téléphone, les shaders voyagent DANS l'APK (voir `androidassets` du
	// descripteur jenga). Jenga y dépose le CONTENU du dossier embarqué à la
	// racine des ressources, alors que le moteur les demande sous
	// « Resources/NKRenderer/Shaders/… » : on déclare donc le sous-dossier à
	// retirer, et NkFile fait la correspondance seul.
	//
	// L'ordre n'est pas un détail — c'est le défaut que je viens de payer. Posé
	// APRÈS la création du renderer, ce réglage arrivait trop tard : les
	// shaders chargés pendant l'initialisation recevaient une source VIDE,
	// échouaient tous à compiler, et le rendu refusait de démarrer. Seuls les
	// matériaux, chargés plus tard, s'en tiraient — ce qui rendait le symptôme
	// trompeur en n'échouant qu'à moitié. Toute déclaration qui gouverne l'accès
	// aux fichiers doit précéder le premier fichier lu.
	NkFile::SetAndroidAssetSubFolder("NKRenderer/Shaders");
#endif

	// ── 1) Le marqueur à imprimer ─────────────────────────────────────────────
	{
		const uint32 size = 512;
		NkImage marker;
		if (marker.Create(size, size, math::NkColor(0, 0, 0, 255), 4)) {
			uint8 *gray = static_cast<uint8 *>(memory::NkAlloc(size * size));
			nkxr::NkArRenderMarker(kMarkerId, 4, gray, size);
			uint8 *pixels = marker.Pixels();
			for (uint32 i = 0; i < size * size; ++i) {
				pixels[i * 4u + 0u] = gray[i];
				pixels[i * 4u + 1u] = gray[i];
				pixels[i * 4u + 2u] = gray[i];
				pixels[i * 4u + 3u] = 255;
			}
			memory::NkFree(gray);
			marker.SaveToFile("nkar_marqueur.png");
			logger.Infof("[NKARDemo] Marqueur à imprimer écrit : nkar_marqueur.png (identifiant %d).\n", kMarkerId);
		}
	}

	// ── 2) Fenêtre + device + renderer ────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "NKARDemo — réalité augmentée à marqueurs (Échap = quitter)";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	NkWindow window(winCfg);
	if (!window.IsValid()) {
		logger.Error("[NKARDemo] Création fenêtre KO");
		return 1;
	}

	NkDeviceInitInfo devInfo{};
	devInfo.surface = window.GetSurfaceDesc();
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;
	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
	if (!device || !device->IsValid()) {
		logger.Error("[NKARDemo] Création device KO");
		window.Close();
		return 2;
	}
	uint32 W = devInfo.width;
	uint32 H = devInfo.height;

	// Un seul renderer : la vidéo est un fond 2D, la 3D se pose dessus.
	NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, W, H);
	cfg.postProcess.bloom = false;
	cfg.postProcess.ssao = false;
	NkRenderer *renderer = NkRenderer::Create(device, cfg);
	if (!renderer) {
		logger.Error("[NKARDemo] Init renderer KO");
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	// ── 3) Caméra ─────────────────────────────────────────────────────────────
	NkCameraSystem &camera = NkCameraSystem::Instance();
	bool cameraOk = false;
	NkCameraConfig camCfg;
	camCfg.width = kCamWidth;
	camCfg.height = kCamHeight;
	camCfg.fps = 30;
	camCfg.outputFormat = NkPixelFormat::NK_PIXEL_RGBA8;
	// Sur téléphone, l'AR se fait par la caméra ARRIÈRE : on vise le marqueur,
	// on ne se filme pas. La façade est donc demandée explicitement, et le
	// choix reste programmable — NkCameraConfig::facing existe déjà, c'était
	// simplement le réglage par défaut « n'importe laquelle » qui décidait.
	camCfg.facing = NkCameraFacing::NK_CAMERA_FACING_BACK;
	{
		const char *facingEnv = getenv("NK_AR_CAMERA");
		if (facingEnv != nullptr && (facingEnv[0] == 'f' || facingEnv[0] == 'F')) {
			camCfg.facing = NkCameraFacing::NK_CAMERA_FACING_FRONT;
		}
	}
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
	// Demander AVANT d'ouvrir la caméra, et laisser à l'utilisateur le temps de
	// répondre. On n'attend pas indéfiniment : si la réponse tarde, la démo
	// bascule sur son image de synthèse et le DIT, plutôt que de figer sa boucle
	// — Android tue une application qui ne répond pas au bout de dix secondes.
	{
		bool granted = NkAndroidRequestCameraPermission();
		for (uint32 wait = 0; !granted && wait < 40u; ++wait) {
			NkChrono::Sleep(int64(150));
			granted = NkAndroidRequestCameraPermission();
		}
		logger.Infof("[NKARDemo] Permission camera : %s\n", granted ? "ACCORDEE" : "refusee ou sans reponse");
	}
#endif
	if (camera.Init()) {
		const auto devices = camera.EnumerateDevices();
		// Dire CE QU'ON A TROUVÉ, une ligne par appareil : sur téléphone il y en
		// a plusieurs (grand angle, ultra grand angle, façade), et savoir lequel
		// a été ouvert évite d'accuser la détection quand c'est l'objectif qui
		// regarde ailleurs.
		for (nk_size i = 0; i < devices.Size(); ++i) {
			logger.Infof("[NKARDemo] Camera %u : \"%s\" facade=%u\n", uint32(devices[i].index),
						 devices[i].name.CStr(), uint32(devices[i].facing));
		}
		if (devices.Size() > 0) {
			cameraOk = camera.StartStreaming(camCfg);
			logger.Infof("[NKARDemo] Caméra : %u périphérique(s), facade demandee=%u, flux %s.\n",
						 uint32(devices.Size()), uint32(camCfg.facing), cameraOk ? "démarré" : "REFUSÉ");
		}
	}
	if (!cameraOk) {
		// Le dire franchement plutôt que d'afficher un écran noir : la chaîne
		// AR reste exercée sur une image de synthèse animée.
		logger.Warn("[NKARDemo] Aucune caméra exploitable — repli sur une image de SYNTHÈSE animée. "
					"La chaîne AR complète reste exercée.");
	}

	// ── 4) Session AR — TOUT PAR LE CODE ──────────────────────────────────────
	nkxr::NkArSessionConfig arCfg;
	arCfg.markerSizeMeters = 0.10f;      // côté du carré noir imprimé
	arCfg.fallbackFovXDegrees = 60.f;    // webcam typique, non calibrée
	arCfg.lostToleranceFrames = 8;
	arCfg.smoothing = 0.35f;
	arCfg.detector.minEdgePixels = 40;
	// L'environnement n'est qu'un raccourci de développement, explicite :
	{
		const char *sizeEnv = getenv("NK_AR_MARKER_SIZE");
		if (sizeEnv != nullptr && *sizeEnv != '\0') {
			arCfg.markerSizeMeters = float32(atof(sizeEnv));
		}
		arCfg.detector.debugCounters = (getenv("NK_AR_DEBUG") != nullptr);
		// Seuillage adaptatif : à essayer quand le marqueur est affiché sur un
		// ÉCRAN (halos) ou éclairé de biais — c'est exactement son terrain.
		arCfg.detector.adaptive = (getenv("NK_AR_ADAPTIVE") != nullptr);
		// Mode ANCRE : montrer la carte UNE FOIS pose la scene, qui reste
		// ensuite en place — on peut ranger la carte. C'est le modele
		// « carte -> systeme solaire ». Valable camera FIXE (poste, borne).
		if (getenv("NK_AR_ANCHOR") != nullptr) {
			arCfg.anchorMode = nkxr::NkArAnchorMode::NK_AR_ANCHOR_PERSISTENT;
		}
	}
	// Le MONDE : repere commun, stable, independant de la camera. Les objets y
	// sont poses une fois et y restent — on peut bouger l'objectif, sortir,
	// revenir : ils sont a leur place.
	nkxr::NkArWorld arWorld;
	{
		// Un objet posé sur une table ne s'estompe pas parce qu'on détourne le
		// regard : il RESTE, et l'on n'en voit que la part qui tombe dans le
		// champ. La démo ne fait donc jamais disparaître un objet du monde — la
		// seule chose qui décide de sa visibilité est le champ de vision, par
		// découpage. (maxBlindFrames existe pour les applications qui préfèrent
		// cacher plutôt que de montrer une place qui vieillit ; ici, non.)
		nkxr::NkArWorldConfig worldCfg;
		worldCfg.maxBlindFrames = 0;
		arWorld.Initialize(worldCfg);
	}

	uint32 arWidth = kCamWidth;
	uint32 arHeight = kCamHeight;
	bool loggedFormat = false;
	nkxr::NkArSession arSession;
	if (!arSession.Initialize(arCfg, arWidth, arHeight)) {
		logger.Error("[NKARDemo] Init session AR KO");
		return 4;
	}
	// Dire l'état EFFECTIF des réglages : une variable d'environnement survit à
	// toute une session de terminal, et un réglage hérité d'un essai précédent
	// explique bien des « ça ne marche plus » — vécu ici même, l'adaptatif
	// resté armé rendait le masque bruité et le code illisible.
	logger.Infof("[NKARDemo] Marqueur attendu : %.1f cm de côté | seuillage : %s.\n",
				 arCfg.markerSizeMeters * 100.f, arCfg.detector.adaptive ? "ADAPTATIF" : "Otsu global");

	// Texture vidéo : mise à jour à chaque image (pas recréée — recréer une
	// texture par frame ferait tousser l'allocateur GPU en quelques secondes).
	NkTextureCreateDesc texDesc;
	texDesc.width = kCamWidth;
	texDesc.height = kCamHeight;
	texDesc.format = NkGPUFormat::NK_RGBA8_UNORM;
	texDesc.debugName = "NKARDemo_Video";
	NkTexHandle videoTex = renderer->GetTextures()->Create(texDesc);

	// Tampon de travail : les octets que l'on donne à la session AR.
	uint8 *frameRGBA = static_cast<uint8 *>(memory::NkAlloc(nk_size(kCamWidth) * kCamHeight * 4u));
	uint8 *pattern = static_cast<uint8 *>(memory::NkAlloc(256u * 256u));
	nkxr::NkArRenderMarker(kMarkerId, 4, pattern, 256);

	// ── 5) Boucle ─────────────────────────────────────────────────────────────
	bool running = true;
	NkEventSystem &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE) {
			running = false;
		}
		// R = oublier les ancres : le pendant indispensable du mode ancré —
		// une scène posée par erreur doit pouvoir être retirée.
		if (e->GetKey() == NkKey::NK_R) {
			arSession.ForgetAll();
			arWorld.RemoveAll();
			logger.Info("[NKARDemo] Ancres et objets du monde retirés (touche R).");
		}
		// ESPACE : poser un objet DANS LE MONDE, à 60 cm devant l'objectif.
		// Il y restera : bouger la caméra ne le déplacera pas.
		if (e->GetKey() == NkKey::NK_SPACE) {
			const uint32 handle = arWorld.PlaceInFrontOfCamera(0.6f);
			if (handle != 0u) {
				logger.Infof("[NKARDemo] Objet %u posé dans le monde (%u au total).\n", handle,
							 uint32(arWorld.GetAnchors().Size()));
			}
			else {
				logger.Warn("[NKARDemo] Impossible de poser : aucun marqueur n'a encore défini le monde.");
			}
		}
	});
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w >= 64 && h >= 64 && (w != W || h != H)) {
			W = w;
			H = h;
			renderer->OnResize(w, h);
		}
	});

	// Redressement de l'image caméra, en degrés (0, 90, 180, 270).
	// Sur téléphone le capteur est monté de travers : en portrait, l'image
	// arrive couchée. 90° convient à la très grande majorité des dos d'appareils
	// Android ; NK_AR_ROTATE permet de trancher les autres cas sans recompiler,
	// et le réglage reste PROGRAMMABLE (principe n°4) — l'environnement n'est
	// qu'une couche par-dessus.
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
	uint32 kCameraRotation = 90u;
#else
	uint32 kCameraRotation = 0u;
#endif
	{
		const char *rotEnv = getenv("NK_AR_ROTATE");
		if (rotEnv != nullptr) {
			const uint32 asked = uint32(atoi(rotEnv));
			if (asked == 0u || asked == 90u || asked == 180u || asked == 270u) {
				kCameraRotation = asked;
			}
		}
		logger.Infof("[NKARDemo] Redressement image camera : %u degres.\n", kCameraRotation);
	}

	NkClock clock;
	float32 total = 0.f;
	uint64 frameIndex = 0;
	float32 worldMillis = 0.f;
	uint32 lastCameraFrame = 0xFFFFFFFFu;
	uint32 visible = 0;
	uint32 analyzedFrames = 0;
	const uint64 exitFrame = (getenv("NK_AR_EXIT") != nullptr) ? uint64(atoll(getenv("NK_AR_EXIT"))) : 0u;
	const uint64 dumpFrame = (getenv("NK_AR_DUMP") != nullptr) ? uint64(atoll(getenv("NK_AR_DUMP"))) : 0u;
	// Image de synthese forcee : marqueur toujours detectable, sans dependre
	// de la camera ni de l'eclairage — c'est ce qui permet de savoir si un
	// echec vient de la VISION ou de l'AFFICHAGE.
	const bool forceSynthetic = (getenv("NK_AR_SYNTH") != nullptr);

	while (running && window.IsOpen()) {
		events.PollEvents();
		if (!running) {
			break;
		}
		total += clock.Tick().delta;
		++frameIndex;

		// ── Image ────────────────────────────────────────────────────────────
		bool haveFrame = false;
		bool newFrame = false;
		if (cameraOk && !forceSynthetic) {
			NkCameraFrame frame;
			if (camera.GetLastFrame(frame) && frame.IsValid()) {
				// La caméra livre CE QU'ELLE VEUT (YUYV, NV12, MJPEG…) même
				// quand on demande du RGBA : le pilote a le dernier mot. Lire
				// ses octets comme du RGBA donnait une image répétée en
				// mosaïque — un format de 2 octets/pixel lu comme 4 se replie
				// sur lui-même. On CONVERTIT donc, et on le dit une fois.
				if (frame.format != NkPixelFormat::NK_PIXEL_RGBA8) {
					if (!loggedFormat) {
						logger.Infof("[NKARDemo] La caméra livre le format %d — conversion en RGBA8.\n",
									 int(frame.format));
						loggedFormat = true;
					}
					NkCameraSystem::ConvertToRGBA8(frame);
				}
				// La résolution obtenue peut différer de celle demandée : c'est
				// la CAMÉRA qui décide, on s'adapte au lieu de refuser.
				if (frame.format == NkPixelFormat::NK_PIXEL_RGBA8 && frame.width > 0u && frame.height > 0u) {
					// Le capteur d'un téléphone est monté DE TRAVERS par rapport
					// à l'écran : en portrait, l'image arrive couchée. On la
					// redresse ici, en amont de TOUT le reste — détection
					// comprise — sinon le marqueur serait cherché dans une image
					// tournée et les poses sortiraient dans un repère penché.
					const bool swapWH = (kCameraRotation == 90u || kCameraRotation == 270u);
					const uint32 effW = swapWH ? frame.height : frame.width;
					const uint32 effH = swapWH ? frame.width : frame.height;
					if (effW != arWidth || effH != arHeight) {
						logger.Infof("[NKARDemo] Résolution caméra réelle : %ux%u (rotation %u° → %ux%u) — session AR "
									 "réinitialisée à cette taille.\n",
									 frame.width, frame.height, kCameraRotation, effW, effH);
						arWidth = effW;
						arHeight = effH;
						memory::NkFree(frameRGBA);
						frameRGBA = static_cast<uint8 *>(memory::NkAlloc(nk_size(arWidth) * arHeight * 4u));
						arSession.Shutdown();
						arSession.Initialize(arCfg, arWidth, arHeight);
						NkTextureCreateDesc redesc;
						redesc.width = arWidth;
						redesc.height = arHeight;
						redesc.format = NkGPUFormat::NK_RGBA8_UNORM;
						redesc.debugName = "NKARDemo_Video";
						videoTex = renderer->GetTextures()->Create(redesc);
					}
					const uint32 stride = frame.stride ? frame.stride : frame.width * 4u;
					if (kCameraRotation == 0u) {
						for (uint32 y = 0; y < frame.height; ++y) {
							const uint8 *src = frame.data.Data() + nk_size(y) * stride;
							uint8 *dst = frameRGBA + nk_size(y) * frame.width * 4u;
							for (uint32 x = 0; x < frame.width * 4u; ++x) {
								dst[x] = src[x];
							}
						}
					}
					else {
						// Rotation par recopie pixel à pixel. Coûteuse en apparence,
						// mais elle a le mérite d'être vraie une fois pour toutes :
						// tout ce qui suit — détection, pose, projection, HUD —
						// travaille sur une image DROITE, sans avoir à connaître
						// l'inclinaison du capteur.
						for (uint32 y = 0; y < frame.height; ++y) {
							const uint8 *src = frame.data.Data() + nk_size(y) * stride;
							for (uint32 x = 0; x < frame.width; ++x) {
								uint32 dx = 0, dy = 0;
								if (kCameraRotation == 90u) {
									dx = frame.height - 1u - y;
									dy = x;
								}
								else if (kCameraRotation == 180u) {
									dx = frame.width - 1u - x;
									dy = frame.height - 1u - y;
								}
								else { // 270
									dx = y;
									dy = frame.width - 1u - x;
								}
								uint8 *dst = frameRGBA + (nk_size(dy) * arWidth + dx) * 4u;
								const uint8 *s = src + nk_size(x) * 4u;
								dst[0] = s[0];
								dst[1] = s[1];
								dst[2] = s[2];
								dst[3] = s[3];
							}
						}
					}
					haveFrame = true;
					// La caméra livre ~30 images/s, la boucle en affiche bien
					// plus : sans ce garde-fou on analysait plusieurs fois la
					// MÊME image. C'était du temps perdu, et surtout cela
					// noyait les mesures du suivi sous des « rien n'a bougé »
					// parfaitement exacts mais trompeurs à la lecture.
					newFrame = (frame.frameIndex != lastCameraFrame);
					lastCameraFrame = frame.frameIndex;
				}
			}
		}
		// L'image de synthèse CONTIENT le marqueur. La donner à analyser pendant
		// que la caméra démarre plantait l'origine du monde sur un marqueur que
		// l'utilisateur n'a jamais montré : au premier plan filmé, la carte
		// contenait déjà « 1 marqueur » et un cube gris flottait sans raison.
		// Elle ne sert donc QUE lorsqu'il n'y a réellement pas de caméra.
		const bool syntheticAllowed = forceSynthetic || !cameraOk;
		if (!haveFrame && syntheticAllowed) {
			SynthesizeFallback(frameRGBA, arWidth, arHeight, pattern, 256, total);
			newFrame = true;
		}
		const bool waitingForCamera = (!haveFrame && !syntheticAllowed);

		// ── AR : l'image entre, les poses sortent ────────────────────────────
		// Uniquement sur une image NEUVE : analyser deux fois la même n'apprend
		// rien et fausse la lecture du suivi.
		const bool analyze = newFrame && !waitingForCamera;
		if (analyze) {
			visible = arSession.ProcessFrame(frameRGBA, arWidth, arHeight, arWidth * 4u,
											 nkxr::NkArImageFormat::NK_AR_RGBA8);
		}
		renderer->GetTextures()->Update(videoTex, frameRGBA, arWidth * 4u);
		// Le monde se met à jour APRÈS la détection : il en tire la pose de la
		// caméra et étend sa carte. Le coût est MESURÉ et annoncé : le suivi par
		// l'image compare deux images entières, c'est le poste le plus lourd de
		// la chaîne AR et il serait malhonnête de le laisser dans l'ombre.
		if (analyze) {
			NkClock worldClock;
			worldClock.Tick();
			arWorld.Update(arSession);
			worldMillis += worldClock.Tick().delta * 1000.f;
			++analyzedFrames;
			if (analyzedFrames >= 60u) {
				logger.Infof("[NKARDemo] monde (detection exclue) : %.2f ms par image ANALYSEE (60 images).\n",
							 worldMillis / float32(analyzedFrames));
				worldMillis = 0.f;
				analyzedFrames = 0;
			}
		}

		if (!renderer->BeginFrame()) {
			continue;
		}

		// ── Caméra virtuelle = la VRAIE caméra ───────────────────────────────
		// Les poses des marqueurs sont dans le repère caméra : la caméra
		// virtuelle est donc à l'origine, regardant -Z, avec le MÊME champ que
		// l'objectif — sinon l'objet ne se superposerait pas à l'image.
		const nkxr::NkArCameraIntrinsics &K = arSession.GetIntrinsics();
		NkCamera3DData camData;
		camData.position = { 0.f, 0.f, 0.f };
		camData.target = { 0.f, 0.f, -1.f };
		camData.up = { 0.f, 1.f, 0.f };
		camData.nearPlane = 0.01f;
		camData.farPlane = 50.f;
		camData.useFovAsym = true;
		// Le frustum décentré (étage 1) sert ici sa VRAIE raison d'être : le
		// centre optique d'un objectif n'est jamais exactement au milieu.
		camData.fovLeft = -math::NkAtan(K.cx / K.fx);
		camData.fovRight = math::NkAtan((float32(arWidth) - K.cx) / K.fx);
		camData.fovUp = math::NkAtan(K.cy / K.fy);
		camData.fovDown = -math::NkAtan((float32(arHeight) - K.cy) / K.fy);

		NkSceneContext sctx;
		sctx.camera = NkCamera3D(camData);
		sctx.time = total;
		sctx.ambientIntensity = 0.45f;
		NkLightDesc key;
		key.type = NkLightType::NK_DIRECTIONAL;
		key.direction = { -0.3f, -0.7f, -0.6f };
		key.color = { 1.f, 0.97f, 0.92f };
		key.intensity = 2.5f;
		key.castShadow = false; // pas d'ombre : le sol réel n'est pas modélisé
		sctx.lights.PushBack(key);

		NkRender3D *r3d = renderer->GetRender3D();
		r3d->BeginScene(sctx);

		// Rien n'est soumis au rendu 3D : la vidéo est peinte dans la passe
		// overlay, qui vient APRÈS, et cacherait tout. L'augmentation est donc
		// projetée à la main plus bas, avec les intrinsèques de la VRAIE caméra.
		// Un cube 3D soumis ici serait du travail invisible — il l'a été
		// longtemps, et cela brouillait la lecture de ce qui s'affiche vraiment.
		const auto &tracked = arSession.GetTracked();

		// ── Vidéo en fond + HUD ──────────────────────────────────────────────
		NkOverlayRenderer *overlay = renderer->GetOverlay();
		NkRender2D *r2d = renderer->GetRender2D();
		if (overlay && r2d) {
			overlay->BeginOverlay(renderer->GetCmd(), W, H);
			// Proportions de l'image respectées : une vidéo étirée fausserait
			// la lecture de ce que voit la caméra.
			const float32 srcAspect = float32(arWidth) / float32(arHeight);
			NkRectF dst{ 0.f, 0.f, float32(W), float32(H) };
			if (srcAspect > float32(W) / float32(H)) {
				dst.height = float32(W) / srcAspect;
				dst.y = (float32(H) - dst.height) * 0.5f;
			}
			else {
				dst.width = float32(H) * srcAspect;
				dst.x = (float32(W) - dst.width) * 0.5f;
			}
			r2d->DrawImage(videoTex, dst);

			// ── L'AUGMENTATION, dessinée APRÈS la vidéo ──────────────────────
			// Le fond vidéo est peint dans la passe overlay, qui vient APRÈS la
			// 3D : un objet soumis au renderer 3D serait donc CACHÉ par
			// l'image. Tant que la vidéo n'est pas un vrai fond 3D, on projette
			// nous-mêmes l'objet augmenté avec les intrinsèques de la VRAIE
			// caméra — c'est d'ailleurs la superposition la plus exacte
			// possible, puisqu'elle n'emprunte rien à la caméra virtuelle.
			// ── Le cube du marqueur, dessiné DEPUIS LA CARTE DU MONDE ────────
			// Ne PAS le dessiner depuis la pose détectée : celle-ci n'existe que
			// tant que le marqueur est vu, donc l'objet s'évanouirait dès qu'on
			// cache la carte — c'est exactement le défaut constaté. La carte du
			// monde, elle, garde la place du marqueur : le cube reste où il doit
			// tant que la caméra sait où elle est (ou, à défaut, à sa dernière
			// place connue, signalée par une couleur différente).
			const auto &mapEntries = arWorld.GetMap();
			for (nk_size i = 0; i < mapEntries.Size(); ++i) {
				nkxr::NkXrPose posed;
				if (!arWorld.ToCamera(mapEntries[i].poseInWorld, posed)) {
					continue;
				}
				const nkxr::NkArTrackedMarker *live = arSession.Find(mapEntries[i].id);
				const bool seenNow = (live != nullptr && live->visibleThisFrame);
				nkxr::NkArTrackedMarker marker;
				marker.id = mapEntries[i].id;
				marker.pose = posed;
				marker.visibleThisFrame = seenNow;
				const float32 side = arCfg.markerSizeMeters;
				const float32 half = side * 0.5f;
				// Un cube POSÉ sur le marqueur : le marqueur est le sol de
				// l'objet (plan z=0 du marqueur), pas son centre.
				const math::NkVec3f local[8] = {
					{ -half, -half, 0.f },   { half, -half, 0.f },   { half, half, 0.f },   { -half, half, 0.f },
					{ -half, -half, side },  { half, -half, side },  { half, half, side },  { -half, half, side },
				};
				// Les sommets restent dans le repère CAMÉRA : le découpage se
				// fait arête par arête, sur le plan rapproché PUIS sur le bord de
				// l'image. Exiger que les 8 sommets soient projetables — ce que
				// faisait la version précédente — faisait disparaître le cube
				// d'un bloc dès qu'un seul coin sortait du champ.
				math::NkVec3f corners[8];
				for (uint32 c = 0; c < 8u; ++c) {
					corners[c] = marker.pose.Transform(local[c]);
				}
				// Quatre états, quatre couleurs — pour que l'utilisateur sache
				// toujours à quel point il peut se fier à ce qu'il voit :
				//   orange = le marqueur est VU, la pose est mesurée à l'instant ;
				//   bleu   = il est caché, mais la caméra se repère sur un AUTRE
				//            marqueur : la place affichée reste juste ;
				//   jaune  = plus aucun marqueur, la ROTATION de la caméra est
				//            mesurée sur l'image : juste en rotation, dérive si
				//            l'on se déplace ;
				//   gris   = plus rien du tout, dernière place connue.
				// L'opacité suit la CONFIANCE : l'objet s'efface à mesure que la
				// pose vieillit, au lieu de rester net puis de sauter dans le
				// néant. C'est la traduction visuelle de « je sais de moins en
				// moins où tu es ».
				const float32 alpha = arWorld.GetPoseConfidence();
				NkVec4f color = marker.visibleThisFrame
									? NkVec4f{ 1.f, 0.75f, 0.2f, 1.f }
									: (arWorld.IsLocalizedNow()
										   ? NkVec4f{ 0.35f, 0.6f, 1.f, 1.f }
										   : (arWorld.IsTrackingByImage() ? NkVec4f{ 0.95f, 0.9f, 0.25f, 1.f }
																		  : NkVec4f{ 0.65f, 0.65f, 0.65f, 1.f }));
				color.w = alpha;
				const uint32 edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
											  { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
											  { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
				for (uint32 e = 0; e < 12u; ++e) {
					DrawEdge3D(r2d, corners[edges[e][0]], corners[edges[e][1]], K, arWidth, arHeight, dst, color, 3.f);
				}
				// Les axes du marqueur : rouge = X, vert = Y, bleu = Z (la
				// normale). Ils rendent l'ORIENTATION lisible d'un coup d'œil.
				const math::NkVec3f axisOrigin = marker.pose.Transform({ 0.f, 0.f, 0.f });
				DrawEdge3D(r2d, axisOrigin, marker.pose.Transform({ half, 0.f, 0.f }), K, arWidth, arHeight, dst,
						   { 1.f, 0.2f, 0.2f, alpha }, 4.f);
				DrawEdge3D(r2d, axisOrigin, marker.pose.Transform({ 0.f, half, 0.f }), K, arWidth, arHeight, dst,
						   { 0.2f, 1.f, 0.2f, alpha }, 4.f);
				DrawEdge3D(r2d, axisOrigin, marker.pose.Transform({ 0.f, 0.f, half }), K, arWidth, arHeight, dst,
						   { 0.3f, 0.5f, 1.f, alpha }, 4.f);
			}

			// Dire la source RÉELLEMENT affichée, pas celle qu'on espérait :
			// avec NK_AR_SYNTH la caméra est ouverte mais son image n'est PAS
			// utilisée — annoncer « camera » ferait chercher un marqueur dans
			// la pièce alors que celui de l'image de synthèse est à l'écran.
			const char *sourceName = haveFrame ? "CAMERA"
											   : (forceSynthetic ? "SYNTHESE (forcee par NK_AR_SYNTH)"
																 : "SYNTHESE (aucune image camera)");
			// ── Les objets POSÉS DANS LE MONDE ──────────────────────────────
			// Ils ne suivent pas le marqueur : ils occupent une place dans la
			// pièce. Bouger la caméra les fait bouger DANS L'IMAGE — c'est le
			// signe qu'ils sont bien fixes dans le monde, et non collés à
			// l'objectif.
			const auto &worldAnchors = arWorld.GetAnchors();
			for (nk_size i = 0; i < worldAnchors.Size(); ++i) {
				nkxr::NkXrPose inCamera;
				if (!arWorld.GetAnchorInCamera(worldAnchors[i].handle, inCamera)) {
					continue;
				}
				const float32 side = 0.10f;
				const float32 half = side * 0.5f;
				const math::NkVec3f local[8] = {
					{ -half, -half, -half }, { half, -half, -half }, { half, half, -half }, { -half, half, -half },
					{ -half, -half, half },  { half, -half, half },  { half, half, half },  { -half, half, half },
				};
				math::NkVec3f corners[8];
				for (uint32 c = 0; c < 8u; ++c) {
					corners[c] = inCamera.Transform(local[c]);
				}
				// Vert quand la caméra est localisée maintenant, gris quand la
				// position affichée repose sur une localisation qui vieillit :
				// l'utilisateur doit pouvoir douter au bon moment.
				NkVec4f color = arWorld.IsLocalizedNow() ? NkVec4f{ 0.3f, 1.f, 0.45f, 1.f }
														 : NkVec4f{ 0.6f, 0.6f, 0.6f, 1.f };
				color.w = arWorld.GetPoseConfidence();
				const uint32 edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
											  { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
				for (uint32 e = 0; e < 12u; ++e) {
					DrawEdge3D(r2d, corners[edges[e][0]], corners[edges[e][1]], K, arWidth, arHeight, dst, color, 2.5f);
				}
			}

			overlay->DrawText({ 12.f, 20.f }, "NKARDemo — source : %s | marqueurs vus : %u / suivis : %u",
							  sourceName, visible, uint32(tracked.Size()));
			overlay->DrawText({ 12.f, 40.f }, "ESPACE = poser un objet dans le monde | R = tout retirer | "
											  "monde : %s, %u marqueur(s) en carte, %u objet(s)",
							  arWorld.IsLocalizedNow()
								  ? "LOCALISE (marqueur)"
								  : (arWorld.IsTrackingByImage()
										 ? "suivi par l'IMAGE (rotation seule)"
										 : (!arWorld.IsPoseUsable()
												? "PERDU — objets caches"
												: (arWorld.HasEverLocalized() ? "localisation qui vieillit"
																			  : "pas encore d'origine"))),
							  uint32(arWorld.GetMap().Size()), uint32(worldAnchors.Size()));
			// Ce que le suivi par l'image mesure RÉELLEMENT, chiffres à l'appui :
			// sans cela, impossible de distinguer « la caméra ne bouge pas » de
			// « le suivi ne voit rien » — les deux donnent la même image figée.
			{
				const nkxr::NkArFlowResult &flow = arWorld.GetLastFlow();
				const math::NkVec3f cumul = arWorld.GetBlindRotationDeg();
				// « attend » n'est PAS une panne : le suivi compare à une image de
				// référence et ne conclut qu'une fois le glissement sorti du
				// bruit. Le dire évite de croire à un blocage.
				overlay->DrawText({ 12.f, 60.f },
								  "  image : %s | %u trouves, %u ambigus, %u retenus | glissement %.2f px | CUMUL "
								  "%.1f deg (tangage %.1f) | residu %.2f px",
								  flow.inliers > 0u ? (flow.valid ? "CONCLUT" : "suit, attend de sortir du bruit")
													: "rien a suivre (surface unie ?)",
								  flow.candidates, flow.ambiguous, flow.inliers, flow.medianShiftPixels, cumul.y,
								  cumul.x, flow.residualPixels);
			}
			for (nk_size i = 0; i < mapEntries.Size(); ++i) {
				nkxr::NkXrPose posed;
				if (!arWorld.ToCamera(mapEntries[i].poseInWorld, posed)) {
					continue;
				}
				const nkxr::NkArTrackedMarker *live = arSession.Find(mapEntries[i].id);
				const bool seenNow = (live != nullptr && live->visibleThisFrame);
				overlay->DrawText({ 12.f, 80.f + 20.f * float32(i) }, "  id %d : %.2f m devant — %s",
								  mapEntries[i].id, -posed.position.z,
								  seenNow ? "VU (cube orange)"
										  : (arWorld.IsLocalizedNow()
												 ? "cache, mais la camera se repere ailleurs (cube bleu)"
												 : (arWorld.IsTrackingByImage()
														? "cache : rotation suivie sur l'image (cube jaune)"
														: "cache : derniere place connue (cube gris)")));
			}
			overlay->EndOverlay();
		}

		renderer->Present();
		renderer->EndFrame();

		// ── Vidage de diagnostic : ce que le detecteur VOIT ─────────────────
		// Une image du gris et une du masque seuille : si le marqueur est
		// lisible sur la premiere mais absent de la seconde, le probleme est
		// le SEUILLAGE ; s'il manque aux deux, c'est l'optique ou le cadrage.
		if (dumpFrame != 0 && frameIndex == dumpFrame) {
			NkImage img;
			if (img.Create(arWidth, arHeight, math::NkColor(0, 0, 0, 255), 4)) {
				uint8 *px = img.Pixels();
				const uint8 *gray = arSession.GetGray();
				if (gray != nullptr) {
					for (uint32 i = 0; i < arWidth * arHeight; ++i) {
						px[i * 4u + 0u] = gray[i];
						px[i * 4u + 1u] = gray[i];
						px[i * 4u + 2u] = gray[i];
						px[i * 4u + 3u] = 255;
					}
					img.SaveToFile("nkar_diag_gris.png");
				}
				const uint8 *mask = arSession.GetMask();
				if (mask != nullptr) {
					for (uint32 i = 0; i < arWidth * arHeight; ++i) {
						px[i * 4u + 0u] = mask[i];
						px[i * 4u + 1u] = mask[i];
						px[i * 4u + 2u] = mask[i];
						px[i * 4u + 3u] = 255;
					}
					img.SaveToFile("nkar_diag_masque.png");
				}
				logger.Infof("[NKARDemo] Diagnostic ecrit : nkar_diag_gris.png et nkar_diag_masque.png.\n");
			}
		}

		if (exitFrame != 0 && frameIndex >= exitFrame) {
			running = false;
		}
	}

	// ── 6) Fermeture ──────────────────────────────────────────────────────────
	device->WaitIdle();
	memory::NkFree(frameRGBA);
	memory::NkFree(pattern);
	arSession.Shutdown();
	if (cameraOk) {
		camera.StopStreaming();
	}
	camera.Shutdown();
	NkRenderer::Destroy(renderer);
	NkDeviceFactory::Destroy(device);
	window.Close();
	logger.Info("[NKARDemo] Terminé proprement.");
	return 0;
}
