//
// NkXrOpenXRBackend.cpp
// =============================================================================
// Description :
//   Backend OpenXR de NKXR. Étape 2a : point d'entrée (loader dynamique ou
//   découverte directe du runtime), instance, système. Étape 2b.1 : liaison
//   Vulkan (le runtime impose extensions et physical device), session réelle,
//   machine d'états pilotée par les événements du runtime, boucle
//   xrWaitFrame/Begin/End (SANS couches — l'image dans le casque est la
//   2b.2), espaces de référence et poses réelles via xrLocateViews.
//
// Caractéristiques :
//   - XR_NO_PROTOTYPES : aucun symbole lié, tout est résolu à l'exécution.
//   - Temps : les XrTime du runtime (epoch runtime) transitent TELS QUELS
//     dans NkXrTime — l'app ne doit dater ses requêtes de pose qu'avec le
//     predictedDisplayTime du WaitFrame, jamais avec NkXrSession::Now().
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/Backend/NkXrOpenXRBackend.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#define XR_NO_PROTOTYPES 1
#define XR_USE_GRAPHICS_API_VULKAN 1
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
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
				XrSession session = XR_NULL_HANDLE;
				XrViewConfigurationView views[2]{};
				// Espaces de référence : [0]=VIEW [1]=LOCAL [2]=STAGE — même
				// ordre que NkXrSpaceType pour un mapping direct.
				XrSpace spaces[3]{ XR_NULL_HANDLE, XR_NULL_HANDLE, XR_NULL_HANDLE };
				bool vulkanEnableExt = false;
				bool graphicsRequirementsQueried = false;
				bool sessionRunning = false;

				// Fonctions d'instance.
				PFN_xrDestroyInstance destroyInstance = nullptr;
				PFN_xrGetInstanceProperties getInstanceProperties = nullptr;
				PFN_xrGetSystem getSystem = nullptr;
				PFN_xrGetSystemProperties getSystemProperties = nullptr;
				PFN_xrEnumerateViewConfigurationViews enumViews = nullptr;
				PFN_xrPollEvent pollEvent = nullptr;
				// XR_KHR_vulkan_enable.
				PFN_xrGetVulkanGraphicsRequirementsKHR getVkRequirements = nullptr;
				PFN_xrGetVulkanInstanceExtensionsKHR getVkInstanceExt = nullptr;
				PFN_xrGetVulkanDeviceExtensionsKHR getVkDeviceExt = nullptr;
				PFN_xrGetVulkanGraphicsDeviceKHR getVkGraphicsDevice = nullptr;
				// Session.
				PFN_xrCreateSession createSession = nullptr;
				PFN_xrDestroySession destroySession = nullptr;
				PFN_xrBeginSession beginSession = nullptr;
				PFN_xrEndSession endSession = nullptr;
				PFN_xrRequestExitSession requestExitSession = nullptr;
				PFN_xrCreateReferenceSpace createReferenceSpace = nullptr;
				PFN_xrDestroySpace destroySpace = nullptr;
				PFN_xrLocateSpace locateSpace = nullptr;
				PFN_xrWaitFrame waitFrame = nullptr;
				PFN_xrBeginFrame beginFrame = nullptr;
				PFN_xrEndFrame endFrame = nullptr;
				PFN_xrLocateViews locateViews = nullptr;
				// Swapchains (2b.2).
				PFN_xrEnumerateSwapchainFormats enumSwapchainFormats = nullptr;
				PFN_xrCreateSwapchain createSwapchain = nullptr;
				PFN_xrDestroySwapchain destroySwapchain = nullptr;
				PFN_xrEnumerateSwapchainImages enumSwapchainImages = nullptr;
				PFN_xrAcquireSwapchainImage acquireSwapchainImage = nullptr;
				PFN_xrWaitSwapchainImage waitSwapchainImage = nullptr;
				PFN_xrReleaseSwapchainImage releaseSwapchainImage = nullptr;

				// ── 2b.2 : swapchains du casque + copie Vulkan ───────────────
				XrSwapchain hmdSwapchains[2]{ XR_NULL_HANDLE, XR_NULL_HANDLE };
				VkImage hmdImages[2][8]{};
				uint32 hmdImageCount[2]{ 0, 0 };
				uint32 hmdWidth = 0;
				uint32 hmdHeight = 0;
				int64 hmdFormat = 0;
				bool layerPending = false;
				XrCompositionLayerProjectionView pendingViews[2]{};
				// Couche de PROFONDEUR (XR_KHR_composition_layer_depth) : elle
				// donne au compositeur de quoi reprojeter en TRANSLATION et
				// pas seulement en rotation — une frame manquee se voit alors
				// a peine. Les structures doivent survivre jusqu'a EndFrame.
				bool depthLayerExt = false;
				XrSwapchain depthSwapchains[2]{ XR_NULL_HANDLE, XR_NULL_HANDLE };
				VkImage depthImages[2][8]{};
				uint32 depthImageCount[2]{ 0, 0 };
				XrCompositionLayerDepthInfoKHR pendingDepth[2]{};
				bool depthPending = false;

				// Vulkan dynamique : AUCUN link — vulkan-1.dll chargée comme le
				// loader, mêmes raisons (une DLL absente ne doit pas empêcher
				// l'exe de démarrer).
				HMODULE vulkanLib = nullptr;
				VkDevice vkDevice = VK_NULL_HANDLE;
				VkQueue vkQueue = VK_NULL_HANDLE;
				VkCommandPool vkCmdPool = VK_NULL_HANDLE;
				VkCommandBuffer vkCmd = VK_NULL_HANDLE;
				VkFence vkFence = VK_NULL_HANDLE;
				bool vkFenceUsed = false;
				PFN_vkGetDeviceQueue fnGetDeviceQueue = nullptr;
				PFN_vkCreateCommandPool fnCreateCommandPool = nullptr;
				PFN_vkDestroyCommandPool fnDestroyCommandPool = nullptr;
				PFN_vkAllocateCommandBuffers fnAllocateCommandBuffers = nullptr;
				PFN_vkBeginCommandBuffer fnBeginCommandBuffer = nullptr;
				PFN_vkEndCommandBuffer fnEndCommandBuffer = nullptr;
				PFN_vkResetCommandBuffer fnResetCommandBuffer = nullptr;
				PFN_vkQueueSubmit fnQueueSubmit = nullptr;
				PFN_vkCreateFence fnCreateFence = nullptr;
				PFN_vkDestroyFence fnDestroyFence = nullptr;
				PFN_vkWaitForFences fnWaitForFences = nullptr;
				PFN_vkResetFences fnResetFences = nullptr;
				PFN_vkCmdPipelineBarrier fnCmdPipelineBarrier = nullptr;
				PFN_vkCmdCopyImage fnCmdCopyImage = nullptr;

				// ── 2b.3 : actions réelles (manettes) ────────────────────────
				static constexpr uint32 kMaxActions = 16u;
				XrActionSet actionSet = XR_NULL_HANDLE;
				NkXrActionDesc actionDescs[kMaxActions]{};
				uint32 actionCount = 0;
				XrAction actionHandles[kMaxActions]{};
				XrSpace actionSpaces[kMaxActions]{};
				bool actionsAttached = false;
				PFN_xrStringToPath stringToPath = nullptr;
				PFN_xrCreateActionSet createActionSet = nullptr;
				PFN_xrDestroyActionSet destroyActionSet = nullptr;
				PFN_xrCreateAction createAction = nullptr;
				PFN_xrSuggestInteractionProfileBindings suggestBindings = nullptr;
				PFN_xrAttachSessionActionSets attachActionSets = nullptr;
				PFN_xrSyncActions syncActions = nullptr;
				PFN_xrGetActionStateBoolean getActionStateBoolean = nullptr;
				PFN_xrGetActionStateFloat getActionStateFloat = nullptr;
				PFN_xrGetActionStateVector2f getActionStateVector2f = nullptr;
				PFN_xrCreateActionSpace createActionSpace = nullptr;
				PFN_xrApplyHapticFeedback applyHapticFeedback = nullptr;

				// ── Vraies mains (XR_EXT_hand_tracking) ──────────────────────
				bool handTrackingExt = false;       ///< Extension offerte par le runtime.
				bool handTrackingSupported = false; ///< …ET supportée par le système.
				XrHandTrackerEXT handTrackers[2]{ XR_NULL_HANDLE, XR_NULL_HANDLE };
				PFN_xrCreateHandTrackerEXT createHandTracker = nullptr;
				PFN_xrDestroyHandTrackerEXT destroyHandTracker = nullptr;
				PFN_xrLocateHandJointsEXT locateHandJoints = nullptr;

				// ── Mesure et réglage de la restitution ──────────────────────
				bool perfMetricsExt = false;
				bool perfMetricsArmed = false;
				bool refreshRateExt = false;
				bool visibilityMaskExt = false;
				PFN_xrSetPerformanceMetricsStateMETA setPerfState = nullptr;
				PFN_xrQueryPerformanceMetricsCounterMETA queryPerfCounter = nullptr;
				PFN_xrEnumerateDisplayRefreshRatesFB enumRefreshRates = nullptr;
				PFN_xrGetDisplayRefreshRateFB getRefreshRate = nullptr;
				PFN_xrRequestDisplayRefreshRateFB requestRefreshRate = nullptr;
				PFN_xrGetVisibilityMaskKHR getVisibilityMask = nullptr;
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

			NkXrSessionState MapSessionState(XrSessionState state) {
				switch (state) {
					case XR_SESSION_STATE_IDLE: return NkXrSessionState::NK_XR_STATE_IDLE;
					case XR_SESSION_STATE_READY: return NkXrSessionState::NK_XR_STATE_READY;
					case XR_SESSION_STATE_SYNCHRONIZED: return NkXrSessionState::NK_XR_STATE_SYNCHRONIZED;
					case XR_SESSION_STATE_VISIBLE: return NkXrSessionState::NK_XR_STATE_VISIBLE;
					case XR_SESSION_STATE_FOCUSED: return NkXrSessionState::NK_XR_STATE_FOCUSED;
					case XR_SESSION_STATE_STOPPING: return NkXrSessionState::NK_XR_STATE_STOPPING;
					// Perte du runtime = fin de partie, au même titre qu'EXITING.
					case XR_SESSION_STATE_LOSS_PENDING:
					case XR_SESSION_STATE_EXITING:
					default: return NkXrSessionState::NK_XR_STATE_EXITING;
				}
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

			// 3) Extensions d'instance : XR_KHR_vulkan_enable est la clef de
			// l'étape 2b (le runtime dicte ses exigences Vulkan par elle).
			PFN_xrEnumerateInstanceExtensionProperties enumInstanceExt = nullptr;
			mOxr->getProc(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
						  reinterpret_cast<PFN_xrVoidFunction *>(&enumInstanceExt));
			if (enumInstanceExt != nullptr) {
				uint32 extCount = 0;
				enumInstanceExt(nullptr, 0, &extCount, nullptr);
				if (extCount > 0u && extCount < 256u) {
					// Tableau local à taille bornée : pas d'allocation pour
					// une énumération one-shot.
					XrExtensionProperties props[256];
					for (uint32 i = 0; i < extCount; ++i) {
						props[i] = XrExtensionProperties{};
						props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
					}
					enumInstanceExt(nullptr, extCount, &extCount, props);
					// NK_XR_LIST_EXT : dresser la liste de ce que le runtime
					// offre VRAIMENT — la seule façon de trancher entre « notre
					// code ne demande pas » et « le runtime ne propose pas »
					// (cas vécu : suivi des mains absent via Link).
					const bool listExtensions = (getenv("NK_XR_LIST_EXT") != nullptr);
					for (uint32 i = 0; i < extCount; ++i) {
						if (listExtensions) {
							logger.Infof("[NKXR/OpenXR]   extension : %s (v%u)\n", props[i].extensionName,
										 props[i].extensionVersion);
						}
						if (strcmp(props[i].extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) == 0) {
							mOxr->vulkanEnableExt = true;
						}
						if (strcmp(props[i].extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME) == 0) {
							mOxr->handTrackingExt = true;
						}
						if (strcmp(props[i].extensionName, XR_META_PERFORMANCE_METRICS_EXTENSION_NAME) == 0) {
							mOxr->perfMetricsExt = true;
						}
						if (strcmp(props[i].extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME) == 0) {
							mOxr->refreshRateExt = true;
						}
						if (strcmp(props[i].extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME) == 0) {
							mOxr->visibilityMaskExt = true;
						}
						if (strcmp(props[i].extensionName, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME) == 0) {
							mOxr->depthLayerExt = true;
						}
					}
					logger.Infof("[NKXR/OpenXR] %u extensions offertes par le runtime (NK_XR_LIST_EXT=1 pour la liste).\n",
								 extCount);
				}
			}
			if (!mOxr->vulkanEnableExt) {
				logger.Warnf("[NKXR/OpenXR] XR_KHR_vulkan_enable absent : sonde 2a possible, session 2b impossible.\n");
			}

			// 4) Instance : la poignée de main protocolaire.
			PFN_xrCreateInstance createInstance = nullptr;
			mOxr->getProc(XR_NULL_HANDLE, "xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction *>(&createInstance));
			if (createInstance == nullptr) {
				logger.Errorf("[NKXR/OpenXR] xrCreateInstance introuvable.\n");
				Shutdown();
				return false;
			}
			// Demander SEULEMENT ce que le runtime a annoncé : une extension
			// inconnue fait échouer xrCreateInstance en bloc.
			const char *enabledExtensions[8];
			uint32 enabledExtensionCount = 0;
			if (mOxr->vulkanEnableExt) {
				enabledExtensions[enabledExtensionCount] = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
				++enabledExtensionCount;
			}
			if (mOxr->handTrackingExt) {
				enabledExtensions[enabledExtensionCount] = XR_EXT_HAND_TRACKING_EXTENSION_NAME;
				++enabledExtensionCount;
			}
			if (mOxr->perfMetricsExt) {
				enabledExtensions[enabledExtensionCount] = XR_META_PERFORMANCE_METRICS_EXTENSION_NAME;
				++enabledExtensionCount;
			}
			if (mOxr->refreshRateExt) {
				enabledExtensions[enabledExtensionCount] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
				++enabledExtensionCount;
			}
			if (mOxr->visibilityMaskExt) {
				enabledExtensions[enabledExtensionCount] = XR_KHR_VISIBILITY_MASK_EXTENSION_NAME;
				++enabledExtensionCount;
			}
			if (mOxr->depthLayerExt) {
				enabledExtensions[enabledExtensionCount] = XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME;
				++enabledExtensionCount;
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
			createInfo.enabledExtensionCount = enabledExtensionCount;
			createInfo.enabledExtensionNames = enabledExtensions;
			const XrResult createResult = createInstance(&createInfo, &mOxr->instance);
			if (XR_FAILED(createResult)) {
				logger.Errorf("[NKXR/OpenXR] xrCreateInstance a échoué (XrResult %d).\n", int(createResult));
				Shutdown();
				return false;
			}

			// 5) Fonctions d'instance, système HMD, tailles recommandées.
			#define NK_OXR_LOAD(fn, member) \
				mOxr->getProc(mOxr->instance, #fn, reinterpret_cast<PFN_xrVoidFunction *>(&mOxr->member))
			NK_OXR_LOAD(xrDestroyInstance, destroyInstance);
			NK_OXR_LOAD(xrGetInstanceProperties, getInstanceProperties);
			NK_OXR_LOAD(xrGetSystem, getSystem);
			NK_OXR_LOAD(xrGetSystemProperties, getSystemProperties);
			NK_OXR_LOAD(xrEnumerateViewConfigurationViews, enumViews);
			NK_OXR_LOAD(xrPollEvent, pollEvent);
			if (mOxr->vulkanEnableExt) {
				NK_OXR_LOAD(xrGetVulkanGraphicsRequirementsKHR, getVkRequirements);
				NK_OXR_LOAD(xrGetVulkanInstanceExtensionsKHR, getVkInstanceExt);
				NK_OXR_LOAD(xrGetVulkanDeviceExtensionsKHR, getVkDeviceExt);
				NK_OXR_LOAD(xrGetVulkanGraphicsDeviceKHR, getVkGraphicsDevice);
			}
			NK_OXR_LOAD(xrCreateSession, createSession);
			NK_OXR_LOAD(xrDestroySession, destroySession);
			NK_OXR_LOAD(xrBeginSession, beginSession);
			NK_OXR_LOAD(xrEndSession, endSession);
			NK_OXR_LOAD(xrRequestExitSession, requestExitSession);
			NK_OXR_LOAD(xrCreateReferenceSpace, createReferenceSpace);
			NK_OXR_LOAD(xrDestroySpace, destroySpace);
			NK_OXR_LOAD(xrLocateSpace, locateSpace);
			NK_OXR_LOAD(xrWaitFrame, waitFrame);
			NK_OXR_LOAD(xrBeginFrame, beginFrame);
			NK_OXR_LOAD(xrEndFrame, endFrame);
			NK_OXR_LOAD(xrLocateViews, locateViews);
			NK_OXR_LOAD(xrEnumerateSwapchainFormats, enumSwapchainFormats);
			NK_OXR_LOAD(xrCreateSwapchain, createSwapchain);
			NK_OXR_LOAD(xrDestroySwapchain, destroySwapchain);
			NK_OXR_LOAD(xrEnumerateSwapchainImages, enumSwapchainImages);
			NK_OXR_LOAD(xrAcquireSwapchainImage, acquireSwapchainImage);
			NK_OXR_LOAD(xrWaitSwapchainImage, waitSwapchainImage);
			NK_OXR_LOAD(xrReleaseSwapchainImage, releaseSwapchainImage);
			NK_OXR_LOAD(xrStringToPath, stringToPath);
			NK_OXR_LOAD(xrCreateActionSet, createActionSet);
			NK_OXR_LOAD(xrDestroyActionSet, destroyActionSet);
			NK_OXR_LOAD(xrCreateAction, createAction);
			NK_OXR_LOAD(xrSuggestInteractionProfileBindings, suggestBindings);
			NK_OXR_LOAD(xrAttachSessionActionSets, attachActionSets);
			NK_OXR_LOAD(xrSyncActions, syncActions);
			NK_OXR_LOAD(xrGetActionStateBoolean, getActionStateBoolean);
			NK_OXR_LOAD(xrGetActionStateFloat, getActionStateFloat);
			NK_OXR_LOAD(xrGetActionStateVector2f, getActionStateVector2f);
			NK_OXR_LOAD(xrCreateActionSpace, createActionSpace);
			NK_OXR_LOAD(xrApplyHapticFeedback, applyHapticFeedback);
			if (mOxr->handTrackingExt) {
				NK_OXR_LOAD(xrCreateHandTrackerEXT, createHandTracker);
				NK_OXR_LOAD(xrDestroyHandTrackerEXT, destroyHandTracker);
				NK_OXR_LOAD(xrLocateHandJointsEXT, locateHandJoints);
			}
			if (mOxr->perfMetricsExt) {
				NK_OXR_LOAD(xrSetPerformanceMetricsStateMETA, setPerfState);
				NK_OXR_LOAD(xrQueryPerformanceMetricsCounterMETA, queryPerfCounter);
			}
			if (mOxr->refreshRateExt) {
				NK_OXR_LOAD(xrEnumerateDisplayRefreshRatesFB, enumRefreshRates);
				NK_OXR_LOAD(xrGetDisplayRefreshRateFB, getRefreshRate);
				NK_OXR_LOAD(xrRequestDisplayRefreshRateFB, requestRefreshRate);
			}
			if (mOxr->visibilityMaskExt) {
				NK_OXR_LOAD(xrGetVisibilityMaskKHR, getVisibilityMask);
			}
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
				// FORM_FACTOR_UNAVAILABLE = runtime là, casque absent — sur
				// Quest 2 : Link doit être LANCÉ DEPUIS LE CASQUE.
				logger.Errorf("[NKXR/OpenXR] Pas de casque disponible (XrResult %d — Link lancé dans le casque ?).\n",
							  int(sysResult));
				Shutdown();
				return false;
			}

			XrSystemProperties sysProps{};
			sysProps.type = XR_TYPE_SYSTEM_PROPERTIES;
			// L'extension peut exister au runtime sans que CE système la
			// serve : la vraie réponse est dans les propriétés du système.
			XrSystemHandTrackingPropertiesEXT handProps{};
			handProps.type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT;
			if (mOxr->handTrackingExt) {
				sysProps.next = &handProps;
			}
			if (mOxr->getSystemProperties != nullptr && XR_SUCCEEDED(mOxr->getSystemProperties(mOxr->instance, mOxr->systemId, &sysProps))) {
				snprintf(mSystemName, sizeof(mSystemName), "%s", sysProps.systemName);
				mOxr->handTrackingSupported = mOxr->handTrackingExt && (handProps.supportsHandTracking == XR_TRUE);
			}
			logger.Infof("[NKXR/OpenXR] Suivi des mains : %s.\n",
						 mOxr->handTrackingSupported
							 ? "disponible"
							 : (mOxr->handTrackingExt ? "extension présente mais système non compatible"
													  : "extension absente du runtime"));

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
			mSystemInfo.ipdMeters = desc.ipdMeters; // le vrai IPD se lit dans les poses d'yeux.

			logger.Infof("[NKXR/OpenXR] Système : %s — %ux%u par œil recommandé.\n", mSystemName,
						 mSystemInfo.views[0].recommendedWidth, mSystemInfo.views[0].recommendedHeight);
			mState = NkXrSessionState::NK_XR_STATE_IDLE;
			return true;
		}

		void NkXrOpenXRBackend::Shutdown() {
			if (mOxr == nullptr) {
				return;
			}
			// Outillage de composition d'abord : attendre notre dernier travail
			// GPU avant de détruire quoi que ce soit qu'il référence.
			if (mOxr->vkFenceUsed && mOxr->fnWaitForFences != nullptr) {
				mOxr->fnWaitForFences(mOxr->vkDevice, 1, &mOxr->vkFence, VK_TRUE, UINT64_MAX);
			}
			for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
				if (mOxr->hmdSwapchains[eye] != XR_NULL_HANDLE && mOxr->destroySwapchain != nullptr) {
					mOxr->destroySwapchain(mOxr->hmdSwapchains[eye]);
				}
				if (mOxr->depthSwapchains[eye] != XR_NULL_HANDLE && mOxr->destroySwapchain != nullptr) {
					mOxr->destroySwapchain(mOxr->depthSwapchains[eye]);
				}
			}
			if (mOxr->vkFence != VK_NULL_HANDLE && mOxr->fnDestroyFence != nullptr) {
				mOxr->fnDestroyFence(mOxr->vkDevice, mOxr->vkFence, nullptr);
			}
			if (mOxr->vkCmdPool != VK_NULL_HANDLE && mOxr->fnDestroyCommandPool != nullptr) {
				mOxr->fnDestroyCommandPool(mOxr->vkDevice, mOxr->vkCmdPool, nullptr);
			}
			if (mOxr->vulkanLib != nullptr) {
				FreeLibrary(mOxr->vulkanLib);
			}
			for (uint32 i = 0; i < 2u; ++i) {
				if (mOxr->handTrackers[i] != XR_NULL_HANDLE && mOxr->destroyHandTracker != nullptr) {
					mOxr->destroyHandTracker(mOxr->handTrackers[i]);
				}
			}
			for (uint32 i = 0; i < OxrState::kMaxActions; ++i) {
				if (mOxr->actionSpaces[i] != XR_NULL_HANDLE && mOxr->destroySpace != nullptr) {
					mOxr->destroySpace(mOxr->actionSpaces[i]);
				}
			}
			if (mOxr->actionSet != XR_NULL_HANDLE && mOxr->destroyActionSet != nullptr) {
				mOxr->destroyActionSet(mOxr->actionSet);
			}
			// Repli STAGE→LOCAL = alias : ne pas détruire deux fois le même handle.
			if (mOxr->spaces[2] == mOxr->spaces[1]) {
				mOxr->spaces[2] = XR_NULL_HANDLE;
			}
			for (uint32 i = 0; i < 3u; ++i) {
				if (mOxr->spaces[i] != XR_NULL_HANDLE && mOxr->destroySpace != nullptr) {
					mOxr->destroySpace(mOxr->spaces[i]);
				}
			}
			if (mOxr->session != XR_NULL_HANDLE && mOxr->destroySession != nullptr) {
				mOxr->destroySession(mOxr->session);
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

		// ── Liaison Vulkan (étape 2b) ────────────────────────────────────────

		bool NkXrOpenXRBackend::GetVulkanRequirements(NkXrVulkanRequirements &outRequirements) {
			outRequirements = NkXrVulkanRequirements{};
			if (mOxr == nullptr || !mOxr->vulkanEnableExt || mOxr->getVkRequirements == nullptr) {
				return false;
			}
			// Obligatoire AVANT xrCreateSession (la spec l'exige — un runtime
			// strict refuse la session sinon) ; on en profite pour journaliser
			// la fourchette Vulkan attendue.
			XrGraphicsRequirementsVulkanKHR requirements{};
			requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
			if (XR_FAILED(mOxr->getVkRequirements(mOxr->instance, mOxr->systemId, &requirements))) {
				logger.Errorf("[NKXR/OpenXR] xrGetVulkanGraphicsRequirementsKHR a échoué.\n");
				return false;
			}
			mOxr->graphicsRequirementsQueried = true;
			logger.Infof("[NKXR/OpenXR] Vulkan exigé par le runtime : %u.%u.%u → %u.%u.%u\n",
						 XR_VERSION_MAJOR(requirements.minApiVersionSupported),
						 XR_VERSION_MINOR(requirements.minApiVersionSupported),
						 XR_VERSION_PATCH(requirements.minApiVersionSupported),
						 XR_VERSION_MAJOR(requirements.maxApiVersionSupported),
						 XR_VERSION_MINOR(requirements.maxApiVersionSupported),
						 XR_VERSION_PATCH(requirements.maxApiVersionSupported));

			uint32 written = 0;
			if (mOxr->getVkInstanceExt == nullptr ||
				XR_FAILED(mOxr->getVkInstanceExt(mOxr->instance, mOxr->systemId,
												 uint32(sizeof(outRequirements.instanceExtensions)), &written,
												 outRequirements.instanceExtensions))) {
				logger.Errorf("[NKXR/OpenXR] xrGetVulkanInstanceExtensionsKHR a échoué (ou > 1024 octets).\n");
				return false;
			}
			written = 0;
			if (mOxr->getVkDeviceExt == nullptr ||
				XR_FAILED(mOxr->getVkDeviceExt(mOxr->instance, mOxr->systemId,
											   uint32(sizeof(outRequirements.deviceExtensions)), &written,
											   outRequirements.deviceExtensions))) {
				logger.Errorf("[NKXR/OpenXR] xrGetVulkanDeviceExtensionsKHR a échoué (ou > 1024 octets).\n");
				return false;
			}
			logger.Infof("[NKXR/OpenXR] Extensions instance exigées : %s\n", outRequirements.instanceExtensions);
			logger.Infof("[NKXR/OpenXR] Extensions device exigées   : %s\n", outRequirements.deviceExtensions);
			return true;
		}

		void *NkXrOpenXRBackend::GetVulkanPhysicalDevice(void *vkInstance) {
			if (mOxr == nullptr || mOxr->getVkGraphicsDevice == nullptr || vkInstance == nullptr) {
				return nullptr;
			}
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			if (XR_FAILED(mOxr->getVkGraphicsDevice(mOxr->instance, mOxr->systemId,
													static_cast<VkInstance>(vkInstance), &physicalDevice))) {
				logger.Errorf("[NKXR/OpenXR] xrGetVulkanGraphicsDeviceKHR a échoué.\n");
				return nullptr;
			}
			return static_cast<void *>(physicalDevice);
		}

		bool NkXrOpenXRBackend::BindVulkan(const NkXrVulkanBinding &binding) {
			if (mOxr == nullptr || mOxr->createSession == nullptr) {
				return false;
			}
			if (!mOxr->graphicsRequirementsQueried) {
				logger.Errorf("[NKXR/OpenXR] BindVulkan : GetVulkanRequirements doit précéder (exigence de la spec).\n");
				return false;
			}
			XrGraphicsBindingVulkanKHR graphicsBinding{};
			graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
			graphicsBinding.instance = static_cast<VkInstance>(binding.instance);
			graphicsBinding.physicalDevice = static_cast<VkPhysicalDevice>(binding.physicalDevice);
			graphicsBinding.device = static_cast<VkDevice>(binding.device);
			graphicsBinding.queueFamilyIndex = binding.queueFamilyIndex;
			graphicsBinding.queueIndex = binding.queueIndex;

			XrSessionCreateInfo sessionInfo{};
			sessionInfo.type = XR_TYPE_SESSION_CREATE_INFO;
			sessionInfo.next = &graphicsBinding;
			sessionInfo.systemId = mOxr->systemId;
			const XrResult result = mOxr->createSession(mOxr->instance, &sessionInfo, &mOxr->session);
			if (XR_FAILED(result)) {
				logger.Errorf("[NKXR/OpenXR] xrCreateSession a échoué (XrResult %d).\n", int(result));
				return false;
			}

			// Espaces de référence. STAGE peut manquer (pas de zone de jeu
			// configurée) : repli LOCAL, DIT — l'app verra le sol au mauvais
			// endroit plutôt que pas de poses du tout.
			const XrReferenceSpaceType types[3] = { XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL,
													XR_REFERENCE_SPACE_TYPE_STAGE };
			for (uint32 i = 0; i < 3u; ++i) {
				XrReferenceSpaceCreateInfo spaceInfo{};
				spaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
				spaceInfo.referenceSpaceType = types[i];
				spaceInfo.poseInReferenceSpace.orientation.w = 1.f;
				if (XR_FAILED(mOxr->createReferenceSpace(mOxr->session, &spaceInfo, &mOxr->spaces[i]))) {
					mOxr->spaces[i] = XR_NULL_HANDLE;
					if (types[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
						logger.Warnf("[NKXR/OpenXR] Espace STAGE indisponible — repli sur LOCAL.\n");
						mOxr->spaces[i] = mOxr->spaces[1];
					}
				}
			}
			// Outillage Vulkan de la composition (2b.2) : queue, pool, fence.
			// vulkan-1.dll chargée dynamiquement — même politique que le
			// loader, aucun symbole lié.
			mOxr->vkDevice = static_cast<VkDevice>(binding.device);
			mOxr->vulkanLib = LoadLibraryA("vulkan-1.dll");
			PFN_vkGetInstanceProcAddr vkGipa = nullptr;
			if (mOxr->vulkanLib != nullptr) {
				vkGipa = PFN_vkGetInstanceProcAddr(GetProcAddress(mOxr->vulkanLib, "vkGetInstanceProcAddr"));
			}
			if (vkGipa != nullptr) {
				VkInstance vkInstance = static_cast<VkInstance>(binding.instance);
				#define NK_VKX_LOAD(fn, member) \
					mOxr->member = PFN_##fn(vkGipa(vkInstance, #fn))
				NK_VKX_LOAD(vkGetDeviceQueue, fnGetDeviceQueue);
				NK_VKX_LOAD(vkCreateCommandPool, fnCreateCommandPool);
				NK_VKX_LOAD(vkDestroyCommandPool, fnDestroyCommandPool);
				NK_VKX_LOAD(vkAllocateCommandBuffers, fnAllocateCommandBuffers);
				NK_VKX_LOAD(vkBeginCommandBuffer, fnBeginCommandBuffer);
				NK_VKX_LOAD(vkEndCommandBuffer, fnEndCommandBuffer);
				NK_VKX_LOAD(vkResetCommandBuffer, fnResetCommandBuffer);
				NK_VKX_LOAD(vkQueueSubmit, fnQueueSubmit);
				NK_VKX_LOAD(vkCreateFence, fnCreateFence);
				NK_VKX_LOAD(vkDestroyFence, fnDestroyFence);
				NK_VKX_LOAD(vkWaitForFences, fnWaitForFences);
				NK_VKX_LOAD(vkResetFences, fnResetFences);
				NK_VKX_LOAD(vkCmdPipelineBarrier, fnCmdPipelineBarrier);
				NK_VKX_LOAD(vkCmdCopyImage, fnCmdCopyImage);
				#undef NK_VKX_LOAD
			}
			if (mOxr->fnGetDeviceQueue != nullptr && mOxr->fnCreateCommandPool != nullptr) {
				mOxr->fnGetDeviceQueue(mOxr->vkDevice, binding.queueFamilyIndex, binding.queueIndex, &mOxr->vkQueue);
				VkCommandPoolCreateInfo poolInfo{};
				poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				poolInfo.queueFamilyIndex = binding.queueFamilyIndex;
				if (mOxr->fnCreateCommandPool(mOxr->vkDevice, &poolInfo, nullptr, &mOxr->vkCmdPool) == VK_SUCCESS) {
					VkCommandBufferAllocateInfo allocInfo{};
					allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
					allocInfo.commandPool = mOxr->vkCmdPool;
					allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
					allocInfo.commandBufferCount = 1;
					mOxr->fnAllocateCommandBuffers(mOxr->vkDevice, &allocInfo, &mOxr->vkCmd);
					VkFenceCreateInfo fenceInfo{};
					fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
					mOxr->fnCreateFence(mOxr->vkDevice, &fenceInfo, nullptr, &mOxr->vkFence);
				}
			}
			if (mOxr->vkQueue == VK_NULL_HANDLE || mOxr->vkCmd == VK_NULL_HANDLE) {
				logger.Warnf("[NKXR/OpenXR] Outillage Vulkan de composition indisponible : session OK mais AUCUNE couche possible.\n");
			}

			// Trackers de mains : un par côté, créés une fois la session là.
			if (mOxr->handTrackingSupported && mOxr->createHandTracker != nullptr) {
				const XrHandEXT sides[2] = { XR_HAND_LEFT_EXT, XR_HAND_RIGHT_EXT };
				for (uint32 i = 0; i < 2u; ++i) {
					XrHandTrackerCreateInfoEXT handInfo{};
					handInfo.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT;
					handInfo.hand = sides[i];
					handInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT; // les 26 standards
					if (XR_FAILED(mOxr->createHandTracker(mOxr->session, &handInfo, &mOxr->handTrackers[i]))) {
						mOxr->handTrackers[i] = XR_NULL_HANDLE;
						logger.Warnf("[NKXR/OpenXR] Tracker de main %s indisponible.\n", (i == 0) ? "gauche" : "droite");
					}
				}
			}

			logger.Infof("[NKXR/OpenXR] Session de casque créée (liaison Vulkan OK) — en attente de READY.\n");
			return true;
		}

		bool NkXrOpenXRBackend::CreateHmdSwapchains(uint32 width, uint32 height) {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->createSwapchain == nullptr) {
				return false;
			}
			// Le format est négocié : R8G8B8A8 exigé (SRGB de préférence) car
			// la remise d'image se fait par COPIE BRUTE (vkCmdCopyImage) — nos
			// octets sont déjà encodés sRGB par le tonemap ACES ; un blit avec
			// conversion les ré-encoderait (image délavée, le piège classique).
			int64 formats[64];
			uint32 formatCount = 0;
			mOxr->enumSwapchainFormats(mOxr->session, 64, &formatCount, formats);
			int64 chosen = 0;
			for (uint32 i = 0; i < formatCount && chosen == 0; ++i) {
				if (formats[i] == int64(VK_FORMAT_R8G8B8A8_SRGB)) {
					chosen = formats[i];
				}
			}
			for (uint32 i = 0; i < formatCount && chosen == 0; ++i) {
				if (formats[i] == int64(VK_FORMAT_R8G8B8A8_UNORM)) {
					chosen = formats[i];
				}
			}
			if (chosen == 0) {
				logger.Errorf("[NKXR/OpenXR] Aucun format R8G8B8A8 dans les %u formats du runtime — couches impossibles.\n",
							  formatCount);
				return false;
			}
			for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
				XrSwapchainCreateInfo info{};
				info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
				info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
				info.format = chosen;
				info.sampleCount = 1;
				info.width = width;
				info.height = height;
				info.faceCount = 1;
				info.arraySize = 1;
				info.mipCount = 1;
				if (XR_FAILED(mOxr->createSwapchain(mOxr->session, &info, &mOxr->hmdSwapchains[eye]))) {
					logger.Errorf("[NKXR/OpenXR] xrCreateSwapchain œil %u KO.\n", eye);
					return false;
				}
				XrSwapchainImageVulkanKHR images[8];
				for (uint32 i = 0; i < 8u; ++i) {
					images[i] = XrSwapchainImageVulkanKHR{};
					images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
				}
				uint32 imageCount = 0;
				if (XR_FAILED(mOxr->enumSwapchainImages(mOxr->hmdSwapchains[eye], 8, &imageCount,
														reinterpret_cast<XrSwapchainImageBaseHeader *>(images))) ||
					imageCount == 0u) {
					logger.Errorf("[NKXR/OpenXR] xrEnumerateSwapchainImages œil %u KO.\n", eye);
					return false;
				}
				mOxr->hmdImageCount[eye] = imageCount;
				for (uint32 i = 0; i < imageCount && i < 8u; ++i) {
					mOxr->hmdImages[eye][i] = images[i].image;
				}
			}
			// Swapchains de PROFONDEUR (une par oeil) : meme taille, format
			// D32_SFLOAT — celui de nos cibles NKRenderer, pour que la copie
			// reste une copie et non une conversion.
			if (mOxr->depthLayerExt && (getenv("NK_XR_DEPTH_LAYER") == nullptr || getenv("NK_XR_DEPTH_LAYER")[0] != '0')) {
				for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
					XrSwapchainCreateInfo info{};
					info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
					info.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
					info.format = int64(VK_FORMAT_D32_SFLOAT);
					info.sampleCount = 1;
					info.width = width;
					info.height = height;
					info.faceCount = 1;
					info.arraySize = 1;
					info.mipCount = 1;
					if (XR_FAILED(mOxr->createSwapchain(mOxr->session, &info, &mOxr->depthSwapchains[eye]))) {
						mOxr->depthSwapchains[eye] = XR_NULL_HANDLE;
						logger.Warnf("[NKXR/OpenXR] Swapchain de profondeur œil %u refusée — reprojection en rotation seule.\n", eye);
						continue;
					}
					XrSwapchainImageVulkanKHR images[8];
					for (uint32 i = 0; i < 8u; ++i) {
						images[i] = XrSwapchainImageVulkanKHR{};
						images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
					}
					uint32 imageCount = 0;
					if (XR_SUCCEEDED(mOxr->enumSwapchainImages(mOxr->depthSwapchains[eye], 8, &imageCount,
															   reinterpret_cast<XrSwapchainImageBaseHeader *>(images)))) {
						mOxr->depthImageCount[eye] = imageCount;
						for (uint32 i = 0; i < imageCount && i < 8u; ++i) {
							mOxr->depthImages[eye][i] = images[i].image;
						}
					}
				}
				if (mOxr->depthSwapchains[0] != XR_NULL_HANDLE) {
					logger.Infof("[NKXR/OpenXR] Couche de profondeur active : reprojection POSITIONNELLE (moins de sauts).\n");
				}
			}

			mOxr->hmdWidth = width;
			mOxr->hmdHeight = height;
			mOxr->hmdFormat = chosen;
			logger.Infof("[NKXR/OpenXR] Swapchains casque : %ux%u, format %lld, %u images par œil.\n", width, height,
						 (long long)chosen, mOxr->hmdImageCount[0]);
			return true;
		}

		bool NkXrOpenXRBackend::SubmitEyes(const NkXrView views[NK_XR_EYE_COUNT], uint64 nativeImageLeft,
										   uint64 nativeImageRight, uint32 width, uint32 height, uint64 depthLeft,
										   uint64 depthRight, float32 nearZ, float32 farZ) {
			if (mOxr == nullptr || !mOxr->sessionRunning || mOxr->hmdSwapchains[0] == XR_NULL_HANDLE ||
				mOxr->vkQueue == VK_NULL_HANDLE || mOxr->vkCmd == VK_NULL_HANDLE) {
				return false;
			}
			if (width != mOxr->hmdWidth || height != mOxr->hmdHeight) {
				// Copie brute = tailles STRICTEMENT égales ; l'app doit rendre
				// à la taille des swapchains, pas « à peu près ».
				logger.Errorf("[NKXR/OpenXR] SubmitEyes : %ux%u fourni, %ux%u attendu.\n", width, height,
							  mOxr->hmdWidth, mOxr->hmdHeight);
				return false;
			}
			const VkImage sources[2] = { VkImage(uintptr_t(nativeImageLeft)), VkImage(uintptr_t(nativeImageRight)) };

			// Attendre la copie de la frame PRÉCÉDENTE, pas celle-ci : le CPU
			// garde une frame d'avance. La correction ne repose pas sur cette
			// attente mais sur l'ORDRE DE SOUMISSION — notre copie part sur la
			// MÊME file graphique que le rendu, donc elle s'exécute forcément
			// après le rendu qui l'a précédée et avant celui qui la suit.
			// Mesuré (XR_META_performance_metrics) : attendre tout de suite
			// coûtait 10 à 24 ms de CPU BLOQUÉ pour 0,01 ms de GPU réel.
			// NK_XR_SYNC_COPY=1 rétablit l'attente immédiate (diagnostic).
			if (mOxr->vkFenceUsed) {
				mOxr->fnWaitForFences(mOxr->vkDevice, 1, &mOxr->vkFence, VK_TRUE, UINT64_MAX);
				mOxr->fnResetFences(mOxr->vkDevice, 1, &mOxr->vkFence);
				mOxr->vkFenceUsed = false;
			}
			mOxr->fnResetCommandBuffer(mOxr->vkCmd, 0);
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			mOxr->fnBeginCommandBuffer(mOxr->vkCmd, &beginInfo);

			uint32 acquired[2] = { 0, 0 };
			for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
				XrSwapchainImageAcquireInfo acquireInfo{};
				acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
				if (XR_FAILED(mOxr->acquireSwapchainImage(mOxr->hmdSwapchains[eye], &acquireInfo, &acquired[eye]))) {
					return false;
				}
				XrSwapchainImageWaitInfo waitInfo{};
				waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
				waitInfo.timeout = XR_INFINITE_DURATION;
				if (XR_FAILED(mOxr->waitSwapchainImage(mOxr->hmdSwapchains[eye], &waitInfo))) {
					return false;
				}
				const VkImage dst = mOxr->hmdImages[eye][acquired[eye]];
				const VkImage src = sources[eye];

				VkImageSubresourceRange range{};
				range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				range.levelCount = 1;
				range.layerCount = 1;

				// Source : la cible finale du renderer sort échantillonnable
				// (le compositeur fenêtre la lit en Overlay2D) → SHADER_READ.
				VkImageMemoryBarrier barriers[2]{};
				barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				// MEMORY_WRITE et pas SHADER_READ : ce sont les ÉCRITURES du
				// rendu qu'il faut rendre visibles au transfert — ne flusher
				// que les lectures laissait la copie lire des données
				// partielles (déchirures possibles sous contention).
				barriers[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
				barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[0].image = src;
				barriers[0].subresourceRange = range;
				// Destination : contenu intégralement réécrit → UNDEFINED, le
				// GPU n'a rien à préserver.
				barriers[1] = barriers[0];
				barriers[1].srcAccessMask = 0;
				barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barriers[1].image = dst;
				mOxr->fnCmdPipelineBarrier(mOxr->vkCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
										   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

				VkImageCopy region{};
				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.layerCount = 1;
				region.dstSubresource = region.srcSubresource;
				region.extent = { width, height, 1 };
				mOxr->fnCmdCopyImage(mOxr->vkCmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst,
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				// Retours : source ré-échantillonnable, destination dans le
				// layout que XR_KHR_vulkan_enable exige à la remise.
				barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				mOxr->fnCmdPipelineBarrier(mOxr->vkCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
										   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

				// ── Profondeur : meme geste, aspect DEPTH ────────────────────
				const uint64 depthSources[2] = { depthLeft, depthRight };
				if (mOxr->depthSwapchains[eye] != XR_NULL_HANDLE && depthSources[eye] != 0u) {
					XrSwapchainImageAcquireInfo dAcquire{};
					dAcquire.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
					uint32 dIndex = 0;
					if (XR_SUCCEEDED(mOxr->acquireSwapchainImage(mOxr->depthSwapchains[eye], &dAcquire, &dIndex))) {
						XrSwapchainImageWaitInfo dWait{};
						dWait.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
						dWait.timeout = XR_INFINITE_DURATION;
						mOxr->waitSwapchainImage(mOxr->depthSwapchains[eye], &dWait);

						const VkImage dDst = mOxr->depthImages[eye][dIndex];
						const VkImage dSrc = VkImage(uintptr_t(depthSources[eye]));
						VkImageSubresourceRange dRange{};
						dRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
						dRange.levelCount = 1;
						dRange.layerCount = 1;

						VkImageMemoryBarrier dBar[2]{};
						dBar[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						dBar[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
						dBar[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						dBar[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
						dBar[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						dBar[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						dBar[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						dBar[0].image = dSrc;
						dBar[0].subresourceRange = dRange;
						dBar[1] = dBar[0];
						dBar[1].srcAccessMask = 0;
						dBar[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						dBar[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
						dBar[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						dBar[1].image = dDst;
						mOxr->fnCmdPipelineBarrier(mOxr->vkCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
												   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, dBar);

						VkImageCopy dRegion{};
						dRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
						dRegion.srcSubresource.layerCount = 1;
						dRegion.dstSubresource = dRegion.srcSubresource;
						dRegion.extent = { width, height, 1 };
						mOxr->fnCmdCopyImage(mOxr->vkCmd, dSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dDst,
											 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &dRegion);

						dBar[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						dBar[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
						dBar[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						dBar[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
						dBar[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						dBar[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
						dBar[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						dBar[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
						mOxr->fnCmdPipelineBarrier(mOxr->vkCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
												   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, dBar);

						XrSwapchainImageReleaseInfo dRelease{};
						dRelease.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
						mOxr->releaseSwapchainImage(mOxr->depthSwapchains[eye], &dRelease);

						mOxr->pendingDepth[eye] = XrCompositionLayerDepthInfoKHR{};
						mOxr->pendingDepth[eye].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
						mOxr->pendingDepth[eye].subImage.swapchain = mOxr->depthSwapchains[eye];
						mOxr->pendingDepth[eye].subImage.imageRect.extent.width = int32(width);
						mOxr->pendingDepth[eye].subImage.imageRect.extent.height = int32(height);
						// minDepth/maxDepth = la plage ECRITE dans le tampon ;
						// nearZ/farZ = les plans de la camera. Le compositeur
						// en deduit la distance reelle de chaque pixel.
						mOxr->pendingDepth[eye].minDepth = 0.f;
						mOxr->pendingDepth[eye].maxDepth = 1.f;
						mOxr->pendingDepth[eye].nearZ = nearZ;
						mOxr->pendingDepth[eye].farZ = farZ;
						mOxr->depthPending = true;
					}
				}
			}

			mOxr->fnEndCommandBuffer(mOxr->vkCmd);
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &mOxr->vkCmd;
			mOxr->fnQueueSubmit(mOxr->vkQueue, 1, &submitInfo, mOxr->vkFence);
			mOxr->vkFenceUsed = true;
			// Diagnostic : rétablir l'attente immédiate si l'on soupçonne un
			// jour une file séparée (l'ordre de soumission ne protégerait plus).
			static const bool syncCopy = (getenv("NK_XR_SYNC_COPY") != nullptr);
			if (syncCopy) {
				mOxr->fnWaitForFences(mOxr->vkDevice, 1, &mOxr->vkFence, VK_TRUE, UINT64_MAX);
				mOxr->fnResetFences(mOxr->vkDevice, 1, &mOxr->vkFence);
				mOxr->vkFenceUsed = false;
			}

			for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
				// Release APRÈS la soumission : le runtime considère alors tout
				// le travail GPU en file comme faisant partie de la frame.
				XrSwapchainImageReleaseInfo releaseInfo{};
				releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
				mOxr->releaseSwapchainImage(mOxr->hmdSwapchains[eye], &releaseInfo);

				mOxr->pendingViews[eye] = XrCompositionLayerProjectionView{};
				mOxr->pendingViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
				mOxr->pendingViews[eye].pose.position.x = views[eye].position.x;
				mOxr->pendingViews[eye].pose.position.y = views[eye].position.y;
				mOxr->pendingViews[eye].pose.position.z = views[eye].position.z;
				mOxr->pendingViews[eye].pose.orientation.x = views[eye].orientation.x;
				mOxr->pendingViews[eye].pose.orientation.y = views[eye].orientation.y;
				mOxr->pendingViews[eye].pose.orientation.z = views[eye].orientation.z;
				mOxr->pendingViews[eye].pose.orientation.w = views[eye].orientation.w;
				mOxr->pendingViews[eye].fov.angleLeft = views[eye].fov.angleLeft;
				mOxr->pendingViews[eye].fov.angleRight = views[eye].fov.angleRight;
				mOxr->pendingViews[eye].fov.angleUp = views[eye].fov.angleUp;
				mOxr->pendingViews[eye].fov.angleDown = views[eye].fov.angleDown;
				mOxr->pendingViews[eye].subImage.swapchain = mOxr->hmdSwapchains[eye];
				mOxr->pendingViews[eye].subImage.imageRect.extent.width = int32(width);
				mOxr->pendingViews[eye].subImage.imageRect.extent.height = int32(height);
				// La profondeur se CHAINE sur la vue (next) : c'est ainsi que
				// XR_KHR_composition_layer_depth s'attache.
				mOxr->pendingViews[eye].next = mOxr->depthPending ? &mOxr->pendingDepth[eye] : nullptr;
			}
			mOxr->layerPending = true;
			return true;
		}

		NkXrSystemInfo NkXrOpenXRBackend::GetSystemInfo() const {
			return mSystemInfo;
		}

		NkXrSessionState NkXrOpenXRBackend::GetState() const {
			return mState;
		}

		bool NkXrOpenXRBackend::PollEvent(NkXrEvent &outEvent) {
			if (mOxr == nullptr || mOxr->pollEvent == nullptr) {
				return false;
			}
			// On dépile jusqu'au premier événement qui NOUS concerne : les
			// autres (recentrage, perte d'interaction…) viendront quand NKXR
			// saura les représenter — les taire ici serait les perdre, on les
			// journalise donc au passage.
			XrEventDataBuffer event{};
			event.type = XR_TYPE_EVENT_DATA_BUFFER;
			while (mOxr->pollEvent(mOxr->instance, &event) == XR_SUCCESS) {
				if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
					const auto *stateEvent = reinterpret_cast<const XrEventDataSessionStateChanged *>(&event);
					mState = MapSessionState(stateEvent->state);
					outEvent.type = NkXrEventType::NK_XR_EVENT_STATE_CHANGED;
					outEvent.state = mState;
					outEvent.time = NkXrTime(stateEvent->time);
					return true;
				}
				logger.Infof("[NKXR/OpenXR] Événement runtime non mappé (XrStructureType %d).\n", int(event.type));
				event = XrEventDataBuffer{};
				event.type = XR_TYPE_EVENT_DATA_BUFFER;
			}
			return false;
		}

		bool NkXrOpenXRBackend::BeginSession() {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->beginSession == nullptr) {
				logger.Errorf("[NKXR/OpenXR] BeginSession : pas de session de casque (BindVulkan manquant ?).\n");
				return false;
			}
			// Attacher les actions AVANT d'ouvrir la boucle : l'attachement est
			// définitif et doit précéder la première synchronisation.
			SetupActions();
			XrSessionBeginInfo beginInfo{};
			beginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
			beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			const XrResult result = mOxr->beginSession(mOxr->session, &beginInfo);
			if (XR_FAILED(result)) {
				logger.Errorf("[NKXR/OpenXR] xrBeginSession a échoué (XrResult %d).\n", int(result));
				return false;
			}
			mOxr->sessionRunning = true;
			return true;
		}

		bool NkXrOpenXRBackend::EndSession() {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->endSession == nullptr) {
				return false;
			}
			mOxr->sessionRunning = false;
			return XR_SUCCEEDED(mOxr->endSession(mOxr->session));
		}

		void NkXrOpenXRBackend::RequestExit() {
			if (mOxr != nullptr && mOxr->session != XR_NULL_HANDLE && mOxr->requestExitSession != nullptr) {
				mOxr->requestExitSession(mOxr->session);
			}
		}

		bool NkXrOpenXRBackend::WaitFrame(NkXrFrameState &outState) {
			if (mOxr == nullptr || !mOxr->sessionRunning || mOxr->waitFrame == nullptr) {
				return false;
			}
			XrFrameState frameState{};
			frameState.type = XR_TYPE_FRAME_STATE;
			XrFrameWaitInfo waitInfo{};
			waitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
			if (XR_FAILED(mOxr->waitFrame(mOxr->session, &waitInfo, &frameState))) {
				return false;
			}
			outState.predictedDisplayTime = NkXrTime(frameState.predictedDisplayTime);
			outState.predictedDisplayPeriod = NkXrTime(frameState.predictedDisplayPeriod);
			outState.shouldRender = (frameState.shouldRender == XR_TRUE);
			return true;
		}

		bool NkXrOpenXRBackend::BeginFrame() {
			if (mOxr == nullptr || !mOxr->sessionRunning || mOxr->beginFrame == nullptr) {
				return false;
			}
			XrFrameBeginInfo beginInfo{};
			beginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
			// XR_FRAME_DISCARDED est un succès : la frame précédente a juste
			// été jetée par le compositeur.
			return XR_SUCCEEDED(mOxr->beginFrame(mOxr->session, &beginInfo));
		}

		bool NkXrOpenXRBackend::EndFrame(const NkXrFrameEndInfo &info) {
			if (mOxr == nullptr || !mOxr->sessionRunning || mOxr->endFrame == nullptr) {
				return false;
			}
			// 2b.2 : la couche de projection préparée par SubmitEyes part ici ;
			// sans elle (démarrage, frame sautée), soumission vide légale.
			XrCompositionLayerProjection projectionLayer{};
			const XrCompositionLayerBaseHeader *layers[1] = { nullptr };
			XrFrameEndInfo endInfo{};
			endInfo.type = XR_TYPE_FRAME_END_INFO;
			endInfo.displayTime = XrTime(info.displayTime);
			endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
			if (mOxr->layerPending) {
				projectionLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
				projectionLayer.space = mOxr->spaces[2]; // STAGE (ou son repli LOCAL)
				projectionLayer.viewCount = 2;
				projectionLayer.views = mOxr->pendingViews;
				layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader *>(&projectionLayer);
				endInfo.layerCount = 1;
				endInfo.layers = layers;
				mOxr->layerPending = false;
				mOxr->depthPending = false;
			}
			else {
				endInfo.layerCount = 0;
				endInfo.layers = nullptr;
			}
			return XR_SUCCEEDED(mOxr->endFrame(mOxr->session, &endInfo));
		}

		bool NkXrOpenXRBackend::LocateViews(NkXrSpaceType space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->locateViews == nullptr) {
				return false;
			}
			XrSpace baseSpace = mOxr->spaces[uint32(space)];
			if (baseSpace == XR_NULL_HANDLE) {
				return false;
			}
			XrViewLocateInfo locateInfo{};
			locateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
			locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			locateInfo.displayTime = XrTime(displayTime);
			locateInfo.space = baseSpace;
			XrViewState viewState{};
			viewState.type = XR_TYPE_VIEW_STATE;
			XrView views[2];
			views[0] = XrView{};
			views[0].type = XR_TYPE_VIEW;
			views[1] = XrView{};
			views[1].type = XR_TYPE_VIEW;
			uint32 viewCount = 0;
			if (XR_FAILED(mOxr->locateViews(mOxr->session, &locateInfo, &viewState, 2, &viewCount, views)) ||
				viewCount != 2) {
				return false;
			}
			// Sans orientation valide, une pose est un mensonge — refuser.
			if ((viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
				return false;
			}
			for (uint32 eye = 0; eye < NK_XR_EYE_COUNT; ++eye) {
				// Conventions IDENTIQUES (main droite, -Z avant, radians
				// signés) : copie directe, aucune transformation — c'est le
				// dividende de l'étage 0.
				outViews[eye].position = NkVec3f(views[eye].pose.position.x, views[eye].pose.position.y,
												 views[eye].pose.position.z);
				outViews[eye].orientation = NkQuatf(views[eye].pose.orientation.x, views[eye].pose.orientation.y,
													views[eye].pose.orientation.z, views[eye].pose.orientation.w);
				outViews[eye].fov.angleLeft = views[eye].fov.angleLeft;
				outViews[eye].fov.angleRight = views[eye].fov.angleRight;
				outViews[eye].fov.angleUp = views[eye].fov.angleUp;
				outViews[eye].fov.angleDown = views[eye].fov.angleDown;
			}
			return true;
		}

		bool NkXrOpenXRBackend::LocateSpace(NkXrSpaceType space, NkXrSpaceType base, NkXrTime time, NkXrPose &outPose) {
			if (mOxr == nullptr || mOxr->locateSpace == nullptr) {
				return false;
			}
			XrSpace target = mOxr->spaces[uint32(space)];
			XrSpace baseSpace = mOxr->spaces[uint32(base)];
			if (target == XR_NULL_HANDLE || baseSpace == XR_NULL_HANDLE) {
				return false;
			}
			XrSpaceLocation location{};
			location.type = XR_TYPE_SPACE_LOCATION;
			if (XR_FAILED(mOxr->locateSpace(target, baseSpace, XrTime(time), &location))) {
				return false;
			}
			if ((location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0) {
				return false;
			}
			outPose.position = NkVec3f(location.pose.position.x, location.pose.position.y, location.pose.position.z);
			outPose.orientation = NkQuatf(location.pose.orientation.x, location.pose.orientation.y,
										  location.pose.orientation.z, location.pose.orientation.w);
			return true;
		}

		// ── Actions réelles : manettes Touch (2b.3) ──────────────────────────

		void NkXrOpenXRBackend::AttachActions(const NkXrActionDesc *actions, uint32 count) {
			if (mOxr == nullptr) {
				return;
			}
			if (count > OxrState::kMaxActions) {
				logger.Warnf("[NKXR/OpenXR] %u actions demandées, %u retenues (kMaxActions).\n", count,
							 OxrState::kMaxActions);
				count = OxrState::kMaxActions;
			}
			mOxr->actionCount = count;
			for (uint32 i = 0; i < count; ++i) {
				mOxr->actionDescs[i] = actions[i];
			}
			if (mOxr->session != XR_NULL_HANDLE) {
				SetupActions();
			}
		}

		void NkXrOpenXRBackend::SetupActions() {
			if (mOxr == nullptr || mOxr->actionsAttached || mOxr->actionCount == 0u ||
				mOxr->session == XR_NULL_HANDLE || mOxr->createActionSet == nullptr || mOxr->stringToPath == nullptr) {
				return;
			}
			XrActionSetCreateInfo setInfo{};
			setInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
			snprintf(setInfo.actionSetName, XR_MAX_ACTION_SET_NAME_SIZE, "nkxr");
			snprintf(setInfo.localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE, "NKXR");
			if (XR_FAILED(mOxr->createActionSet(mOxr->instance, &setInfo, &mOxr->actionSet))) {
				logger.Errorf("[NKXR/OpenXR] xrCreateActionSet KO.\n");
				return;
			}

			for (uint32 i = 0; i < mOxr->actionCount; ++i) {
				const NkXrActionDesc &desc = mOxr->actionDescs[i];
				XrActionCreateInfo actionInfo{};
				actionInfo.type = XR_TYPE_ACTION_CREATE_INFO;
				// Nom technique GÉNÉRÉ (contrainte [a-z0-9_]) ; le nom de
				// l'app, libre, part dans le localisé — unique par l'index.
				snprintf(actionInfo.actionName, XR_MAX_ACTION_NAME_SIZE, "action_%u", i);
				snprintf(actionInfo.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "%u %s", i,
						 (desc.name != nullptr && desc.name[0] != '\0') ? desc.name : "action");
				switch (desc.type) {
					case NkXrActionType::NK_XR_ACTION_BOOL: actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT; break;
					case NkXrActionType::NK_XR_ACTION_FLOAT: actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT; break;
					case NkXrActionType::NK_XR_ACTION_VEC2: actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT; break;
					case NkXrActionType::NK_XR_ACTION_POSE: actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT; break;
					case NkXrActionType::NK_XR_ACTION_HAPTIC: actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT; break;
				}
				if (XR_FAILED(mOxr->createAction(mOxr->actionSet, &actionInfo, &mOxr->actionHandles[i]))) {
					logger.Errorf("[NKXR/OpenXR] xrCreateAction %u KO.\n", i);
					continue;
				}
				if (desc.type == NkXrActionType::NK_XR_ACTION_POSE && mOxr->createActionSpace != nullptr) {
					XrActionSpaceCreateInfo spaceInfo{};
					spaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
					spaceInfo.action = mOxr->actionHandles[i];
					spaceInfo.poseInActionSpace.orientation.w = 1.f;
					mOxr->createActionSpace(mOxr->session, &spaceInfo, &mOxr->actionSpaces[i]);
				}
			}

			// Liaisons par USAGE → chemins du profil : c'est ICI (et seulement
			// ici) que la sémantique NKXR rencontre les chemins OpenXR.
			auto MakePath = [this](const char *text) -> XrPath {
				XrPath path = XR_NULL_PATH;
				mOxr->stringToPath(mOxr->instance, text, &path);
				return path;
			};
			auto TouchBinding = [](NkXrActionUsage usage) -> const char * {
				switch (usage) {
					case NkXrActionUsage::NK_XR_USAGE_SELECT: return "/user/hand/right/input/trigger/value";
					case NkXrActionUsage::NK_XR_USAGE_GRAB: return "/user/hand/right/input/squeeze/value";
					case NkXrActionUsage::NK_XR_USAGE_MENU: return "/user/hand/left/input/menu/click";
					case NkXrActionUsage::NK_XR_USAGE_MOVE: return "/user/hand/left/input/thumbstick";
					case NkXrActionUsage::NK_XR_USAGE_AIM_POSE: return "/user/hand/right/input/aim/pose";
					case NkXrActionUsage::NK_XR_USAGE_GRIP_POSE: return "/user/hand/right/input/grip/pose";
					case NkXrActionUsage::NK_XR_USAGE_HAPTIC: return "/user/hand/right/output/haptic";
				}
				return nullptr;
			};
			auto SimpleBinding = [](NkXrActionUsage usage) -> const char * {
				switch (usage) {
					case NkXrActionUsage::NK_XR_USAGE_SELECT: return "/user/hand/right/input/select/click";
					case NkXrActionUsage::NK_XR_USAGE_MENU: return "/user/hand/left/input/menu/click";
					case NkXrActionUsage::NK_XR_USAGE_AIM_POSE: return "/user/hand/right/input/aim/pose";
					case NkXrActionUsage::NK_XR_USAGE_GRIP_POSE: return "/user/hand/right/input/grip/pose";
					case NkXrActionUsage::NK_XR_USAGE_HAPTIC: return "/user/hand/right/output/haptic";
					default: return nullptr; // pas de stick/squeeze sur le profil simple
				}
			};
			const char *profiles[2] = { "/interaction_profiles/oculus/touch_controller",
										"/interaction_profiles/khr/simple_controller" };
			for (uint32 p = 0; p < 2u; ++p) {
				XrActionSuggestedBinding bindings[OxrState::kMaxActions];
				uint32 bindingCount = 0;
				for (uint32 i = 0; i < mOxr->actionCount; ++i) {
					if (mOxr->actionHandles[i] == XR_NULL_HANDLE) {
						continue;
					}
					const char *path = (p == 0) ? TouchBinding(mOxr->actionDescs[i].usage)
												: SimpleBinding(mOxr->actionDescs[i].usage);
					if (path == nullptr) {
						continue;
					}
					bindings[bindingCount].action = mOxr->actionHandles[i];
					bindings[bindingCount].binding = MakePath(path);
					++bindingCount;
				}
				if (bindingCount == 0u || mOxr->suggestBindings == nullptr) {
					continue;
				}
				XrInteractionProfileSuggestedBinding suggested{};
				suggested.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
				suggested.interactionProfile = MakePath(profiles[p]);
				suggested.countSuggestedBindings = bindingCount;
				suggested.suggestedBindings = bindings;
				if (XR_FAILED(mOxr->suggestBindings(mOxr->instance, &suggested))) {
					logger.Warnf("[NKXR/OpenXR] Liaisons refusées pour %s.\n", profiles[p]);
				}
			}

			XrSessionActionSetsAttachInfo attachInfo{};
			attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
			attachInfo.countActionSets = 1;
			attachInfo.actionSets = &mOxr->actionSet;
			if (mOxr->attachActionSets == nullptr || XR_FAILED(mOxr->attachActionSets(mOxr->session, &attachInfo))) {
				logger.Errorf("[NKXR/OpenXR] xrAttachSessionActionSets KO — entrées manettes indisponibles.\n");
				return;
			}
			mOxr->actionsAttached = true;
			logger.Infof("[NKXR/OpenXR] %u actions attachées (profils Touch + simple).\n", mOxr->actionCount);
		}

		bool NkXrOpenXRBackend::SyncActions(NkXrTime now) {
			(void)now;
			if (mOxr == nullptr || !mOxr->actionsAttached || mOxr->syncActions == nullptr) {
				return false;
			}
			XrActiveActionSet activeSet{};
			activeSet.actionSet = mOxr->actionSet;
			activeSet.subactionPath = XR_NULL_PATH;
			XrActionsSyncInfo syncInfo{};
			syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
			syncInfo.countActiveActionSets = 1;
			syncInfo.activeActionSets = &activeSet;
			// XR_SESSION_NOT_FOCUSED est un code de SUCCÈS : les états passent
			// simplement inactifs — exactement le contrat de notre API.
			return XR_SUCCEEDED(mOxr->syncActions(mOxr->session, &syncInfo));
		}

		bool NkXrOpenXRBackend::GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) {
			if (mOxr == nullptr || handle == NK_XR_ACTION_INVALID || handle > mOxr->actionCount ||
				mOxr->getActionStateBoolean == nullptr) {
				return false;
			}
			const uint32 index = handle - 1u;
			if (mOxr->actionDescs[index].type != NkXrActionType::NK_XR_ACTION_BOOL) {
				return false;
			}
			XrActionStateGetInfo getInfo{};
			getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
			getInfo.action = mOxr->actionHandles[index];
			XrActionStateBoolean state{};
			state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
			if (XR_FAILED(mOxr->getActionStateBoolean(mOxr->session, &getInfo, &state))) {
				return false;
			}
			outState.current = (state.currentState == XR_TRUE);
			outState.changed = (state.changedSinceLastSync == XR_TRUE);
			outState.active = (state.isActive == XR_TRUE);
			outState.lastChangeTime = NkXrTime(state.lastChangeTime);
			return true;
		}

		bool NkXrOpenXRBackend::GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) {
			if (mOxr == nullptr || handle == NK_XR_ACTION_INVALID || handle > mOxr->actionCount ||
				mOxr->getActionStateFloat == nullptr) {
				return false;
			}
			const uint32 index = handle - 1u;
			if (mOxr->actionDescs[index].type != NkXrActionType::NK_XR_ACTION_FLOAT) {
				return false;
			}
			XrActionStateGetInfo getInfo{};
			getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
			getInfo.action = mOxr->actionHandles[index];
			XrActionStateFloat state{};
			state.type = XR_TYPE_ACTION_STATE_FLOAT;
			if (XR_FAILED(mOxr->getActionStateFloat(mOxr->session, &getInfo, &state))) {
				return false;
			}
			outState.current = state.currentState;
			outState.changed = (state.changedSinceLastSync == XR_TRUE);
			outState.active = (state.isActive == XR_TRUE);
			return true;
		}

		bool NkXrOpenXRBackend::GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) {
			if (mOxr == nullptr || handle == NK_XR_ACTION_INVALID || handle > mOxr->actionCount ||
				mOxr->getActionStateVector2f == nullptr) {
				return false;
			}
			const uint32 index = handle - 1u;
			if (mOxr->actionDescs[index].type != NkXrActionType::NK_XR_ACTION_VEC2) {
				return false;
			}
			XrActionStateGetInfo getInfo{};
			getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
			getInfo.action = mOxr->actionHandles[index];
			XrActionStateVector2f state{};
			state.type = XR_TYPE_ACTION_STATE_VECTOR2F;
			if (XR_FAILED(mOxr->getActionStateVector2f(mOxr->session, &getInfo, &state))) {
				return false;
			}
			outState.current = NkVec2f(state.currentState.x, state.currentState.y);
			outState.changed = (state.changedSinceLastSync == XR_TRUE);
			outState.active = (state.isActive == XR_TRUE);
			return true;
		}

		bool NkXrOpenXRBackend::LocateActionPose(NkXrActionHandle handle, NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) {
			if (mOxr == nullptr || handle == NK_XR_ACTION_INVALID || handle > mOxr->actionCount ||
				mOxr->locateSpace == nullptr) {
				return false;
			}
			const uint32 index = handle - 1u;
			if (mOxr->actionDescs[index].type != NkXrActionType::NK_XR_ACTION_POSE ||
				mOxr->actionSpaces[index] == XR_NULL_HANDLE) {
				return false;
			}
			XrSpace baseSpace = mOxr->spaces[uint32(space)];
			if (baseSpace == XR_NULL_HANDLE) {
				return false;
			}
			XrSpaceLocation location{};
			location.type = XR_TYPE_SPACE_LOCATION;
			if (XR_FAILED(mOxr->locateSpace(mOxr->actionSpaces[index], baseSpace, XrTime(time), &location))) {
				return false;
			}
			if ((location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0) {
				return false;
			}
			outPose.position = NkVec3f(location.pose.position.x, location.pose.position.y, location.pose.position.z);
			outPose.orientation = NkQuatf(location.pose.orientation.x, location.pose.orientation.y,
										  location.pose.orientation.z, location.pose.orientation.w);
			return true;
		}

		bool NkXrOpenXRBackend::ApplyHaptic(NkXrActionHandle handle, float32 amplitude, float32 durationSeconds,
											float32 frequencyHz) {
			if (mOxr == nullptr || handle == NK_XR_ACTION_INVALID || handle > mOxr->actionCount ||
				mOxr->applyHapticFeedback == nullptr) {
				return false;
			}
			const uint32 index = handle - 1u;
			if (mOxr->actionDescs[index].type != NkXrActionType::NK_XR_ACTION_HAPTIC) {
				return false;
			}
			XrHapticActionInfo hapticInfo{};
			hapticInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
			hapticInfo.action = mOxr->actionHandles[index];
			XrHapticVibration vibration{};
			vibration.type = XR_TYPE_HAPTIC_VIBRATION;
			vibration.duration = (durationSeconds <= 0.f) ? XR_MIN_HAPTIC_DURATION
														  : XrDuration(float64(durationSeconds) * 1e9);
			vibration.frequency = (frequencyHz <= 0.f) ? XR_FREQUENCY_UNSPECIFIED : frequencyHz;
			vibration.amplitude = math::NkClamp(amplitude, 0.f, 1.f);
			return XR_SUCCEEDED(mOxr->applyHapticFeedback(mOxr->session, &hapticInfo,
														  reinterpret_cast<const XrHapticBaseHeader *>(&vibration)));
		}

		bool NkXrOpenXRBackend::LocateHand(NkXrHandSide side, NkXrSpaceType space, NkXrTime time, NkXrHand &outHand) {
			outHand = NkXrHand{};
			if (mOxr == nullptr || mOxr->locateHandJoints == nullptr) {
				return false;
			}
			const uint32 index = uint32(side);
			if (mOxr->handTrackers[index] == XR_NULL_HANDLE) {
				return false;
			}
			XrSpace baseSpace = mOxr->spaces[uint32(space)];
			if (baseSpace == XR_NULL_HANDLE) {
				return false;
			}
			XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT];
			XrHandJointLocationsEXT locations{};
			locations.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
			locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
			locations.jointLocations = jointLocations;
			XrHandJointsLocateInfoEXT locateInfo{};
			locateInfo.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT;
			locateInfo.baseSpace = baseSpace;
			locateInfo.time = XrTime(time);
			if (XR_FAILED(mOxr->locateHandJoints(mOxr->handTrackers[index], &locateInfo, &locations))) {
				return false;
			}
			// isActive false = main hors champ des caméras : ce n'est pas une
			// erreur, c'est « je ne la vois pas » — l'app doit pouvoir
			// distinguer les deux (false ici, mais sans journal alarmiste).
			if (locations.isActive != XR_TRUE) {
				return false;
			}
			outHand.active = true;
			const uint32 count = (locations.jointCount < NK_XR_HAND_JOINT_COUNT) ? locations.jointCount
																				: NK_XR_HAND_JOINT_COUNT;
			for (uint32 j = 0; j < count; ++j) {
				const XrHandJointLocationEXT &loc = jointLocations[j];
				const bool valid = (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0 &&
								   (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
				outHand.joints[j].valid = valid;
				if (!valid) {
					continue;
				}
				outHand.joints[j].position = NkVec3f(loc.pose.position.x, loc.pose.position.y, loc.pose.position.z);
				outHand.joints[j].orientation = NkQuatf(loc.pose.orientation.x, loc.pose.orientation.y,
													   loc.pose.orientation.z, loc.pose.orientation.w);
				outHand.joints[j].radius = loc.radius;
			}
			return true;
		}

		// ── Métriques du compositeur (XR_META_performance_metrics) ───────────

		bool NkXrOpenXRBackend::GetPerfMetrics(NkXrPerfMetrics &outMetrics) {
			outMetrics = NkXrPerfMetrics{};
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->queryPerfCounter == nullptr ||
				mOxr->stringToPath == nullptr) {
				return false;
			}
			// La collecte doit être ARMÉE une fois : sans ça les compteurs
			// répondent « pas de valeur » et on croirait l'extension muette.
			if (!mOxr->perfMetricsArmed) {
				if (mOxr->setPerfState != nullptr) {
					XrPerformanceMetricsStateMETA state{};
					state.type = XR_TYPE_PERFORMANCE_METRICS_STATE_META;
					state.enabled = XR_TRUE;
					mOxr->setPerfState(mOxr->session, &state);
				}
				mOxr->perfMetricsArmed = true;
			}
			// Chemins normalisés par l'extension ; un compteur absent laisse
			// simplement sa valeur à -1 (« non servi »), jamais un zéro qui
			// se lirait comme « gratuit ».
			struct Counter {
					const char *path;
					float32 *target;
			};
			const Counter counters[] = {
				{ "/perfmetrics_meta/app/cpu_frametime", &outMetrics.appCpuMs },
				{ "/perfmetrics_meta/app/gpu_frametime", &outMetrics.appGpuMs },
				{ "/perfmetrics_meta/compositor/cpu_frametime", &outMetrics.compositorCpuMs },
				{ "/perfmetrics_meta/compositor/gpu_frametime", &outMetrics.compositorGpuMs },
				{ "/perfmetrics_meta/app/motion_to_photon_latency", &outMetrics.appMotionToPhotonMs },
			};
			for (const Counter &counter : counters) {
				XrPath path = XR_NULL_PATH;
				if (XR_FAILED(mOxr->stringToPath(mOxr->instance, counter.path, &path))) {
					continue;
				}
				XrPerformanceMetricsCounterMETA value{};
				value.type = XR_TYPE_PERFORMANCE_METRICS_COUNTER_META;
				if (XR_FAILED(mOxr->queryPerfCounter(mOxr->session, path, &value))) {
					continue;
				}
				if ((value.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_FLOAT_VALUE_VALID_BIT_META) != 0) {
					*counter.target = value.floatValue;
					outMetrics.available = true;
				}
			}
			{
				XrPath path = XR_NULL_PATH;
				if (XR_SUCCEEDED(mOxr->stringToPath(mOxr->instance, "/perfmetrics_meta/compositor/stale_frame_count",
													&path))) {
					XrPerformanceMetricsCounterMETA value{};
					value.type = XR_TYPE_PERFORMANCE_METRICS_COUNTER_META;
					if (XR_SUCCEEDED(mOxr->queryPerfCounter(mOxr->session, path, &value)) &&
						(value.counterFlags & XR_PERFORMANCE_METRICS_COUNTER_UINT_VALUE_VALID_BIT_META) != 0) {
						outMetrics.staleFrames = int32(value.uintValue);
						outMetrics.available = true;
					}
				}
			}
			return outMetrics.available;
		}

		// ── Cadence d'affichage (XR_FB_display_refresh_rate) ─────────────────

		uint32 NkXrOpenXRBackend::GetDisplayRefreshRates(float32 *outRates, uint32 capacity) {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->enumRefreshRates == nullptr ||
				outRates == nullptr || capacity == 0u) {
				return 0;
			}
			uint32 count = 0;
			if (XR_FAILED(mOxr->enumRefreshRates(mOxr->session, capacity, &count, outRates))) {
				return 0;
			}
			return count;
		}

		float32 NkXrOpenXRBackend::GetDisplayRefreshRate() {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->getRefreshRate == nullptr) {
				return 0.f;
			}
			float32 hz = 0.f;
			if (XR_FAILED(mOxr->getRefreshRate(mOxr->session, &hz))) {
				return 0.f;
			}
			return hz;
		}

		bool NkXrOpenXRBackend::RequestDisplayRefreshRate(float32 hz) {
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->requestRefreshRate == nullptr) {
				return false;
			}
			// 0 = « laisse le runtime décider » (valeur prévue par la spec).
			return XR_SUCCEEDED(mOxr->requestRefreshRate(mOxr->session, hz));
		}

		// ── Masque de visibilité (XR_KHR_visibility_mask) ────────────────────

		bool NkXrOpenXRBackend::GetVisibilityMask(NkXrEye eye, NkXrVisibilityMask &outMask) {
			outMask.vertices.Clear();
			outMask.indices.Clear();
			outMask.valid = false;
			if (mOxr == nullptr || mOxr->session == XR_NULL_HANDLE || mOxr->getVisibilityMask == nullptr) {
				return false;
			}
			// Deux passes : le runtime dit d'abord COMBIEN, on alloue, on
			// redemande — le protocole d'énumération d'OpenXR.
			XrVisibilityMaskKHR mask{};
			mask.type = XR_TYPE_VISIBILITY_MASK_KHR;
			if (XR_FAILED(mOxr->getVisibilityMask(mOxr->session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
												  uint32(eye), XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR,
												  &mask))) {
				return false;
			}
			if (mask.vertexCountOutput == 0u || mask.indexCountOutput == 0u) {
				// Un runtime sans lentille à masquer (Link en fenêtre, casque
				// à optiques carrées) rend légitimement zéro : pas une erreur.
				return false;
			}
			outMask.vertices.Resize(mask.vertexCountOutput);
			outMask.indices.Resize(mask.indexCountOutput);
			mask.vertexCapacityInput = mask.vertexCountOutput;
			mask.indexCapacityInput = mask.indexCountOutput;
			// XrVector2f et NkVec2f : deux float32 contigus, même disposition —
			// le reinterpret évite une copie intermédiaire de milliers de
			// sommets à chaque interrogation.
			mask.vertices = reinterpret_cast<XrVector2f *>(&outMask.vertices[0]);
			mask.indices = &outMask.indices[0];
			if (XR_FAILED(mOxr->getVisibilityMask(mOxr->session, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
												  uint32(eye), XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR,
												  &mask))) {
				outMask.vertices.Clear();
				outMask.indices.Clear();
				return false;
			}
			outMask.valid = true;
			return true;
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
			logger.Errorf("[NKXR/OpenXR] Backend OpenXR non porté sur cette plateforme (Android/Quest à venir).\n");
			return false;
		}

		void NkXrOpenXRBackend::Shutdown() {
		}

		bool NkXrOpenXRBackend::GetVulkanRequirements(NkXrVulkanRequirements &outRequirements) {
			outRequirements = NkXrVulkanRequirements{};
			return false;
		}

		void *NkXrOpenXRBackend::GetVulkanPhysicalDevice(void *vkInstance) {
			(void)vkInstance;
			return nullptr;
		}

		bool NkXrOpenXRBackend::BindVulkan(const NkXrVulkanBinding &binding) {
			(void)binding;
			return false;
		}

		bool NkXrOpenXRBackend::CreateHmdSwapchains(uint32 width, uint32 height) {
			(void)width;
			(void)height;
			return false;
		}

		bool NkXrOpenXRBackend::SubmitEyes(const NkXrView views[NK_XR_EYE_COUNT], uint64 nativeImageLeft,
										   uint64 nativeImageRight, uint32 width, uint32 height, uint64 depthLeft,
										   uint64 depthRight, float32 nearZ, float32 farZ) {
			(void)views;
			(void)nativeImageLeft;
			(void)nativeImageRight;
			(void)width;
			(void)height;
			return false;
		}

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

		void NkXrOpenXRBackend::SetupActions() {
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

		bool NkXrOpenXRBackend::ApplyHaptic(NkXrActionHandle handle, float32 amplitude, float32 durationSeconds,
											float32 frequencyHz) {
			(void)handle;
			(void)amplitude;
			(void)durationSeconds;
			(void)frequencyHz;
			return false;
		}

		bool NkXrOpenXRBackend::LocateHand(NkXrHandSide side, NkXrSpaceType space, NkXrTime time, NkXrHand &outHand) {
			(void)side;
			(void)space;
			(void)time;
			outHand = NkXrHand{};
			return false;
		}

		bool NkXrOpenXRBackend::GetPerfMetrics(NkXrPerfMetrics &outMetrics) {
			outMetrics = NkXrPerfMetrics{};
			return false;
		}

		uint32 NkXrOpenXRBackend::GetDisplayRefreshRates(float32 *outRates, uint32 capacity) {
			(void)outRates;
			(void)capacity;
			return 0;
		}

		float32 NkXrOpenXRBackend::GetDisplayRefreshRate() {
			return 0.f;
		}

		bool NkXrOpenXRBackend::RequestDisplayRefreshRate(float32 hz) {
			(void)hz;
			return false;
		}

		bool NkXrOpenXRBackend::GetVisibilityMask(NkXrEye eye, NkXrVisibilityMask &outMask) {
			(void)eye;
			outMask.valid = false;
			return false;
		}

#endif // NKENTSEU_PLATFORM_WINDOWS

	} // namespace xr
} // namespace nkentseu
