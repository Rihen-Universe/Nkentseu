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
					for (uint32 i = 0; i < extCount; ++i) {
						if (strcmp(props[i].extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) == 0) {
							mOxr->vulkanEnableExt = true;
						}
					}
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
			const char *enabledExtensions[1] = { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };
			XrInstanceCreateInfo createInfo{};
			createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
			snprintf(createInfo.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "NKXRDemo");
			snprintf(createInfo.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "Nkentseu");
			createInfo.applicationInfo.applicationVersion = 1;
			createInfo.applicationInfo.engineVersion = 1;
			// 1.0 et non CURRENT : on ne demande que ce qu'on consomme — un
			// runtime 1.0 (vieux Quest) doit rester éligible.
			createInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
			createInfo.enabledExtensionCount = mOxr->vulkanEnableExt ? 1u : 0u;
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
			logger.Infof("[NKXR/OpenXR] Session de casque créée (liaison Vulkan OK) — en attente de READY.\n");
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
			// 2b.1 : AUCUNE couche soumise — le casque affiche le vide, mais
			// la boucle de frame est RÉELLE (cadencée par le compositeur) et
			// les poses prédites arrivent. Les couches viennent avec les
			// swapchains (2b.2).
			XrFrameEndInfo endInfo{};
			endInfo.type = XR_TYPE_FRAME_END_INFO;
			endInfo.displayTime = XrTime(info.displayTime);
			endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
			endInfo.layerCount = 0;
			endInfo.layers = nullptr;
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

#endif // NKENTSEU_PLATFORM_WINDOWS

		// ── Commun aux deux plateformes ──────────────────────────────────────

		void NkXrOpenXRBackend::AttachActions(const NkXrActionDesc *actions, uint32 count) {
			// 2b.3 : les actions passeront par xrSuggestInteractionProfileBindings.
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
