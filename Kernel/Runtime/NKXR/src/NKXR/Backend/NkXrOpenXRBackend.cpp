//
// NkXrOpenXRBackend.cpp
// =============================================================================
// Description :
//   Étape 2a du backend OpenXR : trouver un point d'entrée
//   xrGetInstanceProcAddr (loader dynamique, sinon runtime actif découvert à
//   la main sur Windows), créer l'instance, interroger le système HMD et ses
//   tailles de vue recommandées. Étape 2b (Vulkan/session/frames) à venir.
//
// Caractéristiques :
//   - XR_NO_PROTOTYPES : on ne référence AUCUN symbole à l'édition de liens,
//     tout passe par des pointeurs résolus à l'exécution.
//   - Desktop seulement pour l'instant : Android (Quest) réutilisera le même
//     chemin « loader dynamique » avec le libopenxr_loader.so du SDK Meta.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/Backend/NkXrOpenXRBackend.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#define XR_NO_PROTOTYPES 1
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include <windows.h>
#include <cstdio>  // fopen : lire ~200 octets de manifeste json — une
				   // dépendance NKFileSystem entière serait disproportionnée.
#include <cstring>
#include <cstdlib>

namespace nkentseu {
	namespace xr {

		// ── L'état OpenXR opaque ─────────────────────────────────────────────
		struct NkXrOpenXRBackend::OxrState {
				HMODULE library = nullptr;             ///< loader OU runtime.
				PFN_xrGetInstanceProcAddr getProc = nullptr;
				XrInstance instance = XR_NULL_HANDLE;
				XrSystemId systemId = XR_NULL_SYSTEM_ID;
				XrViewConfigurationView views[2]{};
				// Fonctions d'instance (résolues après xrCreateInstance).
				PFN_xrDestroyInstance destroyInstance = nullptr;
				PFN_xrGetInstanceProperties getInstanceProperties = nullptr;
				PFN_xrGetSystem getSystem = nullptr;
				PFN_xrGetSystemProperties getSystemProperties = nullptr;
				PFN_xrEnumerateViewConfigurationViews enumViews = nullptr;
		};

		namespace {

			// Extrait "library_path" du manifeste json d'un runtime OpenXR.
			// Scan borné et non un vrai parseur : le manifeste est un fichier
			// MACHINE (généré par l'installeur du runtime, ~5 clés plates) —
			// embarquer NKSerialization dans NKXR pour ça inverserait le
			// rapport coût/bénéfice. Si un runtime exotique casse ce scan, le
			// journal donne le chemin du manifeste pour diagnostiquer.
			bool ExtractLibraryPath(const char *json, char *out, nk_size cap) {
				const char *key = strstr(json, "\"library_path\"");
				if (key == nullptr) {
					return false;
				}
				const char *colon = strchr(key + 14, ':');
				if (colon == nullptr) {
					return false;
				}
				const char *first = strchr(colon, '"');
				if (first == nullptr) {
					return false;
				}
				++first;
				const char *last = strchr(first, '"');
				if (last == nullptr || nk_size(last - first) + 1u > cap) {
					return false;
				}
				nk_size i = 0;
				for (const char *c = first; c != last; ++c) {
					// Les manifestes Windows échappent les antislashs.
					if (*c == '\\' && (c + 1) != last && *(c + 1) == '\\') {
						continue;
					}
					out[i] = *c;
					++i;
				}
				out[i] = '\0';
				return i > 0;
			}

			// Découverte directe du runtime ACTIF (ce que fait le loader,
			// réduit à l'essentiel : pas de couches d'API, pas de runtimes
			// multiples) : registre Khronos → manifeste → bibliothèque.
			HMODULE LoadActiveRuntimeDirect(char *outPath, nk_size cap) {
				char manifestPath[512] = {};
				DWORD size = DWORD(sizeof(manifestPath));
				const LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1",
													"ActiveRuntime", RRF_RT_REG_SZ, nullptr, manifestPath, &size);
				if (status != ERROR_SUCCESS || manifestPath[0] == '\0') {
					logger.Infof("[NKXR/OpenXR] Aucun runtime actif au registre (SOFTWARE\\Khronos\\OpenXR\\1).\n");
					return nullptr;
				}
				FILE *file = fopen(manifestPath, "rb");
				if (file == nullptr) {
					logger.Errorf("[NKXR/OpenXR] Manifeste illisible : %s\n", manifestPath);
					return nullptr;
				}
				char json[2048] = {};
				const nk_size read = fread(json, 1, sizeof(json) - 1, file);
				fclose(file);
				json[read] = '\0';
				char libPath[512] = {};
				if (!ExtractLibraryPath(json, libPath, sizeof(libPath))) {
					logger.Errorf("[NKXR/OpenXR] library_path introuvable dans %s\n", manifestPath);
					return nullptr;
				}
				// Chemin relatif = relatif au DOSSIER du manifeste (spec loader).
				char full[1024] = {};
				const bool absolute = (libPath[1] == ':') || (libPath[0] == '\\') || (libPath[0] == '/');
				if (absolute) {
					snprintf(full, sizeof(full), "%s", libPath);
				}
				else {
					snprintf(full, sizeof(full), "%s", manifestPath);
					char *slash = strrchr(full, '\\');
					char *slash2 = strrchr(full, '/');
					if (slash2 > slash) {
						slash = slash2;
					}
					if (slash != nullptr) {
						snprintf(slash + 1, sizeof(full) - nk_size(slash + 1 - full), "%s", libPath);
					}
				}
				HMODULE lib = LoadLibraryA(full);
				if (lib == nullptr) {
					logger.Errorf("[NKXR/OpenXR] Chargement du runtime KO : %s\n", full);
					return nullptr;
				}
				snprintf(outPath, cap, "%s", full);
				return lib;
			}

		} // namespace

		NkXrOpenXRBackend::~NkXrOpenXRBackend() {
			Shutdown();
		}

		bool NkXrOpenXRBackend::Initialize(const NkXrSessionDesc &desc) {
			mDesc = desc;
			mOxr = memory::NkGetDefaultAllocator().New<OxrState>();
			if (mOxr == nullptr) {
				return false;
			}

			// 1) Un loader fourni explicitement ou posé à côté de l'exe.
			char loadedFrom[1024] = "openxr_loader.dll";
			const char *override_ = getenv("NK_XR_OPENXR_LOADER");
			if (override_ != nullptr && *override_ != '\0') {
				mOxr->library = LoadLibraryA(override_);
				snprintf(loadedFrom, sizeof(loadedFrom), "%s", override_);
			}
			if (mOxr->library == nullptr) {
				mOxr->library = LoadLibraryA("openxr_loader.dll");
			}
			if (mOxr->library != nullptr) {
				mOxr->getProc = PFN_xrGetInstanceProcAddr(GetProcAddress(mOxr->library, "xrGetInstanceProcAddr"));
			}

			// 2) Sinon : négociation directe avec le runtime actif.
			if (mOxr->getProc == nullptr) {
				if (mOxr->library != nullptr) {
					FreeLibrary(mOxr->library);
					mOxr->library = nullptr;
				}
				mOxr->library = LoadActiveRuntimeDirect(loadedFrom, sizeof(loadedFrom));
				if (mOxr->library != nullptr) {
					auto negotiate = PFN_xrNegotiateLoaderRuntimeInterface(
						GetProcAddress(mOxr->library, "xrNegotiateLoaderRuntimeInterface"));
					if (negotiate != nullptr) {
						XrNegotiateLoaderInfo loaderInfo{};
						loaderInfo.structType = XR_LOADER_INTERFACE_STRUCT_LOADER_INFO;
						loaderInfo.structVersion = XR_LOADER_INFO_STRUCT_VERSION;
						loaderInfo.structSize = sizeof(loaderInfo);
						loaderInfo.minInterfaceVersion = 1;
						loaderInfo.maxInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
						loaderInfo.minApiVersion = XR_MAKE_VERSION(1, 0, 0);
						// TOUT 1.x, pas la version de nos en-têtes : cette
						// fourchette décrit ce que NOUS savons accueillir, et
						// nous ne consommons que le cœur 1.0 — annoncer
						// 1.1.49 a fait refuser le runtime Meta 1.1.59
						// (constaté par Rihen, journal « ICD version not
						// supported by caller »).
						loaderInfo.maxApiVersion = XR_MAKE_VERSION(1, 0xFFFF, 0xFFFFFFFFu);
						XrNegotiateRuntimeRequest request{};
						request.structType = XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST;
						request.structVersion = XR_RUNTIME_INFO_STRUCT_VERSION;
						request.structSize = sizeof(request);
						if (XR_SUCCEEDED(negotiate(&loaderInfo, &request)) && request.getInstanceProcAddr != nullptr) {
							mOxr->getProc = request.getInstanceProcAddr;
						}
					}
				}
			}

			if (mOxr->getProc == nullptr) {
				logger.Errorf("[NKXR/OpenXR] Ni loader, ni runtime actif — backend indisponible. "
							  "(NK_XR_OPENXR_LOADER pour forcer un chemin de loader.)\n");
				Shutdown();
				return false;
			}
			logger.Infof("[NKXR/OpenXR] Point d'entrée OpenXR chargé depuis : %s\n", loadedFrom);

			// 3) Instance : la poignée de main protocolaire.
			PFN_xrCreateInstance createInstance = nullptr;
			mOxr->getProc(XR_NULL_HANDLE, "xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction *>(&createInstance));
			if (createInstance == nullptr) {
				logger.Errorf("[NKXR/OpenXR] xrCreateInstance introuvable.\n");
				Shutdown();
				return false;
			}
			XrInstanceCreateInfo createInfo{};
			createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
			snprintf(createInfo.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "NKXRDemo");
			snprintf(createInfo.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "Nkentseu");
			createInfo.applicationInfo.applicationVersion = 1;
			createInfo.applicationInfo.engineVersion = 1;
			// 1.0 et non CURRENT : on ne demande que ce qu'on consomme — un
			// runtime 1.0 (vieux Quest) doit rester éligible.
			createInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
			const XrResult createResult = createInstance(&createInfo, &mOxr->instance);
			if (XR_FAILED(createResult)) {
				logger.Errorf("[NKXR/OpenXR] xrCreateInstance a échoué (XrResult %d).\n", int(createResult));
				Shutdown();
				return false;
			}

			// 4) Fonctions d'instance, système HMD, tailles recommandées.
			#define NK_OXR_LOAD(fn, member) \
				mOxr->getProc(mOxr->instance, #fn, reinterpret_cast<PFN_xrVoidFunction *>(&mOxr->member))
			NK_OXR_LOAD(xrDestroyInstance, destroyInstance);
			NK_OXR_LOAD(xrGetInstanceProperties, getInstanceProperties);
			NK_OXR_LOAD(xrGetSystem, getSystem);
			NK_OXR_LOAD(xrGetSystemProperties, getSystemProperties);
			NK_OXR_LOAD(xrEnumerateViewConfigurationViews, enumViews);
			#undef NK_OXR_LOAD

			XrInstanceProperties instProps{};
			instProps.type = XR_TYPE_INSTANCE_PROPERTIES;
			if (mOxr->getInstanceProperties != nullptr && XR_SUCCEEDED(mOxr->getInstanceProperties(mOxr->instance, &instProps))) {
				logger.Infof("[NKXR/OpenXR] Runtime : %s (version %u.%u.%u)\n", instProps.runtimeName,
							 XR_VERSION_MAJOR(instProps.runtimeVersion), XR_VERSION_MINOR(instProps.runtimeVersion),
							 XR_VERSION_PATCH(instProps.runtimeVersion));
			}

			XrSystemGetInfo systemInfo{};
			systemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
			systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			const XrResult sysResult = (mOxr->getSystem != nullptr)
										   ? mOxr->getSystem(mOxr->instance, &systemInfo, &mOxr->systemId)
										   : XR_ERROR_FUNCTION_UNSUPPORTED;
			if (XR_FAILED(sysResult)) {
				// FORM_FACTOR_UNAVAILABLE = runtime là, casque absent/éteint :
				// le cas NORMAL sur un PC de dev — le dire en clair.
				logger.Errorf("[NKXR/OpenXR] Pas de casque disponible (XrResult %d — casque branché/réveillé ?).\n",
							  int(sysResult));
				Shutdown();
				return false;
			}

			XrSystemProperties sysProps{};
			sysProps.type = XR_TYPE_SYSTEM_PROPERTIES;
			if (mOxr->getSystemProperties != nullptr && XR_SUCCEEDED(mOxr->getSystemProperties(mOxr->instance, mOxr->systemId, &sysProps))) {
				snprintf(mSystemName, sizeof(mSystemName), "%s", sysProps.systemName);
			}

			uint32 viewCount = 0;
			mOxr->views[0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
			mOxr->views[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
			if (mOxr->enumViews != nullptr &&
				XR_SUCCEEDED(mOxr->enumViews(mOxr->instance, mOxr->systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
											 2, &viewCount, mOxr->views)) &&
				viewCount == 2) {
				for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
					mSystemInfo.views[eye].recommendedWidth = mOxr->views[eye].recommendedImageRectWidth;
					mSystemInfo.views[eye].recommendedHeight = mOxr->views[eye].recommendedImageRectHeight;
				}
			}
			mSystemInfo.systemName = mSystemName;
			mSystemInfo.ipdMeters = desc.ipdMeters; // le vrai IPD viendra des poses d'yeux (2b).

			logger.Infof("[NKXR/OpenXR] Système : %s — %ux%u par œil recommandé.\n", mSystemName,
						 mSystemInfo.views[0].recommendedWidth, mSystemInfo.views[0].recommendedHeight);
			logger.Warnf("[NKXR/OpenXR] Étape 2a seulement : instance+système OK ; session/swapchains Vulkan = étape 2b.\n");
			mState = NkXrSessionState::NK_XR_STATE_IDLE;
			return true;
		}

		void NkXrOpenXRBackend::Shutdown() {
			if (mOxr == nullptr) {
				return;
			}
			if (mOxr->instance != XR_NULL_HANDLE && mOxr->destroyInstance != nullptr) {
				mOxr->destroyInstance(mOxr->instance);
			}
			if (mOxr->library != nullptr) {
				FreeLibrary(mOxr->library);
			}
			memory::NkGetDefaultAllocator().Delete(mOxr);
			mOxr = nullptr;
			mState = NkXrSessionState::NK_XR_STATE_IDLE;
		}

#else // !NKENTSEU_PLATFORM_WINDOWS — Android (Quest) branchera ce même backend
	  // sur le libopenxr_loader.so de Meta ; les autres OS attendront un besoin.

namespace nkentseu {
	namespace xr {

		struct NkXrOpenXRBackend::OxrState {};

		NkXrOpenXRBackend::~NkXrOpenXRBackend() {
			Shutdown();
		}

		bool NkXrOpenXRBackend::Initialize(const NkXrSessionDesc &desc) {
			mDesc = desc;
			logger.Errorf("[NKXR/OpenXR] Backend OpenXR non porté sur cette plateforme (étape 2b : Android/Quest).\n");
			return false;
		}

		void NkXrOpenXRBackend::Shutdown() {
		}

#endif // NKENTSEU_PLATFORM_WINDOWS

		// ── Commun : ce que l'étape 2a ne couvre PAS encore, dit franchement ─

		NkXrSystemInfo NkXrOpenXRBackend::GetSystemInfo() const {
			return mSystemInfo;
		}

		NkXrSessionState NkXrOpenXRBackend::GetState() const {
			return mState;
		}

		bool NkXrOpenXRBackend::PollEvent(NkXrEvent &outEvent) {
			(void)outEvent;
			return false;
		}

		bool NkXrOpenXRBackend::BeginSession() {
			logger.Errorf("[NKXR/OpenXR] BeginSession : étape 2b (liaison Vulkan) non implémentée.\n");
			return false;
		}

		bool NkXrOpenXRBackend::EndSession() {
			return false;
		}

		void NkXrOpenXRBackend::RequestExit() {
		}

		bool NkXrOpenXRBackend::WaitFrame(NkXrFrameState &outState) {
			(void)outState;
			return false;
		}

		bool NkXrOpenXRBackend::BeginFrame() {
			return false;
		}

		bool NkXrOpenXRBackend::EndFrame(const NkXrFrameEndInfo &info) {
			(void)info;
			return false;
		}

		bool NkXrOpenXRBackend::LocateViews(NkXrSpaceType space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) {
			(void)space;
			(void)displayTime;
			(void)outViews;
			return false;
		}

		bool NkXrOpenXRBackend::LocateSpace(NkXrSpaceType space, NkXrSpaceType base, NkXrTime time, NkXrPose &outPose) {
			(void)space;
			(void)base;
			(void)time;
			(void)outPose;
			return false;
		}

		void NkXrOpenXRBackend::AttachActions(const NkXrActionDesc *actions, uint32 count) {
			(void)actions;
			(void)count;
		}

		bool NkXrOpenXRBackend::SyncActions(NkXrTime now) {
			(void)now;
			return false;
		}

		bool NkXrOpenXRBackend::GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) {
			(void)handle;
			(void)outState;
			return false;
		}

		bool NkXrOpenXRBackend::GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) {
			(void)handle;
			(void)outState;
			return false;
		}

		bool NkXrOpenXRBackend::GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) {
			(void)handle;
			(void)outState;
			return false;
		}

		bool NkXrOpenXRBackend::LocateActionPose(NkXrActionHandle handle, NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) {
			(void)handle;
			(void)space;
			(void)time;
			(void)outPose;
			return false;
		}

	} // namespace xr
} // namespace nkentseu
