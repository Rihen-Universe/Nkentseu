// =============================================================================
// tests/sonde_openxr_blend.cpp — demander au runtime OpenXR ce qu'il annonce,
// sans ouvrir de fenetre ni de renderer.
//
// ⚠️ C'EST UN INSTRUMENT, PAS UN PRODUIT, PAS UNE DEMO.
// Il n'est pas construit par build_tests.sh et n'entre dans aucun total
// d'auto-tests. Il existe parce que NKXRDemo n'atteint pas l'initialisation XR
// sur cette machine (il s'arrete dans NkRendererImpl::Initialize, etape 2), donc
// la selection du mode de fusion restait COMPILEE ET NON EXERCEE. La sonde
// contourne le renderer, pas le probleme : le blocage reste entier et appartient
// a l'agent NKRenderer.
// Ne pas l'etoffer. S'il sert un jour d'exemple minimal aux etudiants, tant
// mieux -- ce n'est pas une raison de lui ajouter quoi que ce soit.
//
// ── CE QU'IL MESURE ──────────────────────────────────────────────────────────
//   1. le loader OpenXR se charge-t-il, et repond-il ;
//   2. la liste des modes de fusion annonces pour PRIMARY_STEREO ;
//   3. la REGLE DE SELECTION de NkXrOpenXRBackend rejouee sur cette liste, et
//      surtout : le mode qu'elle choisit est-il DANS la liste annoncee ?
//      C'est la propriete que le correctif du 2026-08-17 doit garantir, et
//      celle que l'ancien code (OPAQUE ecrit en dur) pouvait violer.
//
// ── CE QU'IL NE MESURE PAS ───────────────────────────────────────────────────
//   - il ne cree PAS de session et ne soumet PAS d'image : il ne prouve donc
//     pas que xrEndFrame accepte le mode, seulement que le mode est annonce ;
//   - il n'exerce PAS le code de NkXrOpenXRBackend. Il REJOUE sa regle, ecrite
//     ici a l'identique. Si la regle change la-bas et pas ici, la sonde ment --
//     c'est sa limite principale et elle est volontaire (lier le backend
//     exigerait le renderer, c'est-a-dire le blocage qu'on contourne) ;
//   - il ne dit RIEN du passthrough. XR_FB_passthrough est absente du runtime PC
//     mesure, et un passthrough Quest exige un APK autonome que ce depot ne
//     produit pas.
//
// ── IL PEUT ECHOUER, ET VOICI COMMENT ────────────────────────────────────────
//   0  le runtime a repondu ET le mode choisi est dans la liste annoncee.
//   1  le loader ne se charge pas, ou n'expose pas xrGetInstanceProcAddr.
//   2  xrCreateInstance / xrGetSystem echoue (pas de runtime, pas de casque).
//   3  l'enumeration echoue, ou annonce ZERO mode.
//   4  ⚠️ LE CAS QUI COMPTE : le mode selectionne n'est PAS dans la liste
//      annoncee. C'est le defaut que le correctif elimine.
//
// ETAT DES CHEMINS, mesure le 2026-08-17 (casque non connecte) :
//   code 2  EXERCE — nominal : runtime negocie, instance creee, pas de casque.
//   code 1  EXERCE — XR_RUNTIME_JSON sur un chemin inexistant.
//   codes 3 et 4  NON EXERCES : ils exigent un casque presente par le runtime.
// Un override NK_XR_OPENXR_LOADER bidon retombe sur la voie 3 et rend 2 : c'est
// le repli attendu, pas un echec de la sonde.
//
// ⚠️ Elle n'a donc JAMAIS rendu 0. Ne pas traiter son premier 0 comme une
// confirmation : le chemin 4 -- celui du defaut vise -- n'aura toujours pas ete
// vu tomber.
//
// Construction (pas de bibliotheque a lier : le loader est ouvert dynamiquement,
// comme le fait NkXrOpenXRBackend) :
//   clang++ -std=c++17 -O2 -IExternals/Libs/NKOpenXR/include \
//       Kernel/Runtime/NKXR/tests/sonde_openxr_blend.cpp -o /tmp/sonde_xr.exe
// =============================================================================
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include <windows.h>

#include <cstdio>
#include <cstring>

// La sonde est un outil de diagnostic hors moteur : elle n'utilise ni NKLogger
// ni les allocateurs, pour ne dependre d'aucune bibliotheque du depot. C'est
// deliberement le seul fichier de NKXR ou <cstdio> est admis.

// Voie 3 du backend : registre Khronos -> manifeste JSON -> DLL du runtime.
// Meme sequence que LoadActiveRuntimeDirect dans NkXrOpenXRBackend.cpp.
static HMODULE ChargerRuntimeActif(char *outPath, size_t cap) {
	char manifeste[1024] = {};
	DWORD taille = sizeof(manifeste);
	// XR_RUNTIME_JSON prime sur le registre (comportement du loader Khronos).
	const char *envJson = getenv("XR_RUNTIME_JSON");
	if (envJson != nullptr && *envJson != '\0') {
		snprintf(manifeste, sizeof(manifeste), "%s", envJson);
	} else if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1", "ActiveRuntime",
							RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, manifeste, &taille) != ERROR_SUCCESS) {
		printf("        aucun ActiveRuntime au registre\n");
		return nullptr;
	}

	FILE *f = fopen(manifeste, "rb");
	if (f == nullptr) {
		printf("        manifeste illisible : %s\n", manifeste);
		return nullptr;
	}
	char json[2048] = {};
	const size_t lus = fread(json, 1, sizeof(json) - 1, f);
	fclose(f);
	json[lus] = '\0';

	const char *cle = strstr(json, "\"library_path\"");
	if (cle == nullptr) {
		printf("        library_path absent de %s\n", manifeste);
		return nullptr;
	}
	const char *deb = strchr(cle + 14, '"');
	if (deb == nullptr) {
		return nullptr;
	}
	++deb;
	char libPath[512] = {};
	size_t n = 0;
	// Les JSON Windows echappent les antislash : ".\\LibOVRRTImpl64_1.dll".
	for (; *deb != '\0' && *deb != '"' && n + 1 < sizeof(libPath); ++deb) {
		if (*deb == '\\' && *(deb + 1) == '\\') {
			++deb;
		}
		libPath[n++] = *deb;
	}
	libPath[n] = '\0';

	// Chemin relatif = relatif au DOSSIER du manifeste (spec loader).
	char complet[1024] = {};
	const bool absolu = (libPath[1] == ':') || (libPath[0] == '\\') || (libPath[0] == '/');
	if (absolu) {
		snprintf(complet, sizeof(complet), "%s", libPath);
	} else {
		snprintf(complet, sizeof(complet), "%s", manifeste);
		char *s1 = strrchr(complet, '\\');
		char *s2 = strrchr(complet, '/');
		char *s = (s2 > s1) ? s2 : s1;
		if (s != nullptr) {
			snprintf(s + 1, sizeof(complet) - size_t(s + 1 - complet), "%s", libPath);
		}
	}
	HMODULE lib = LoadLibraryA(complet);
	if (lib == nullptr) {
		printf("        chargement KO : %s (GetLastError=%lu)\n", complet, GetLastError());
		return nullptr;
	}
	snprintf(outPath, cap, "%s", complet);
	return lib;
}

static const char *NomMode(XrEnvironmentBlendMode m) {
	switch (m) {
		case XR_ENVIRONMENT_BLEND_MODE_OPAQUE: return "OPAQUE (monde reel masque)";
		case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE: return "ADDITIVE (ecran transparent)";
		case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND: return "ALPHA_BLEND (compose)";
		default: return "inconnu";
	}
}

int main() {
	printf("=== sonde OpenXR — modes de fusion ===\n");
	printf("instrument de diagnostic. N'ouvre ni fenetre ni renderer.\n");
	printf("Ne cree PAS de session : n'atteste pas que xrEndFrame accepte le mode.\n\n");

	// ⚠️ LES TROIS VOIES DE NkXrOpenXRBackend, DANS SON ORDRE.
	// La premiere version de cette sonde n'essayait que `openxr_loader.dll`, en
	// concluait « le loader est absent, donc le backend ne peut pas s'initialiser
	// ici », et c'etait FAUX : le moteur n'a jamais dependu du loader Khronos.
	// Il negocie directement avec le runtime actif declare au registre.
	// Une sonde qui n'essaie qu'une des trois voies mesure un chemin dont le
	// moteur ne depend pas.
	HMODULE lib = nullptr;
	PFN_xrGetInstanceProcAddr getProc = nullptr;
	char venuDe[1024] = "openxr_loader.dll";

	// Voie 1 : chemin force.
	const char *override_ = getenv("NK_XR_OPENXR_LOADER");
	if (override_ != nullptr && *override_ != '\0') {
		lib = LoadLibraryA(override_);
		snprintf(venuDe, sizeof(venuDe), "%s", override_);
		printf("voie 1 — NK_XR_OPENXR_LOADER=%s : %s\n", override_, lib ? "charge" : "echec");
	}
	// Voie 2 : loader Khronos pose a cote de l'exe.
	if (lib == nullptr) {
		lib = LoadLibraryA("openxr_loader.dll");
		printf("voie 2 — openxr_loader.dll : %s\n", lib ? "charge" : "absent");
	}
	if (lib != nullptr) {
		getProc = PFN_xrGetInstanceProcAddr(GetProcAddress(lib, "xrGetInstanceProcAddr"));
	}
	// Voie 3 : negociation directe avec le runtime actif (registre Khronos).
	// C'est CELLE-CI qui sert en pratique sur cette machine.
	if (getProc == nullptr) {
		if (lib != nullptr) {
			FreeLibrary(lib);
			lib = nullptr;
		}
		lib = ChargerRuntimeActif(venuDe, sizeof(venuDe));
		printf("voie 3 — runtime actif du registre : %s\n", lib ? venuDe : "indisponible");
		if (lib != nullptr) {
			auto negocier =
				PFN_xrNegotiateLoaderRuntimeInterface(GetProcAddress(lib, "xrNegotiateLoaderRuntimeInterface"));
			if (negocier != nullptr) {
				XrNegotiateLoaderInfo li{};
				li.structType = XR_LOADER_INTERFACE_STRUCT_LOADER_INFO;
				li.structVersion = XR_LOADER_INFO_STRUCT_VERSION;
				li.structSize = sizeof(li);
				li.minInterfaceVersion = 1;
				li.maxInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
				li.minApiVersion = XR_MAKE_VERSION(1, 0, 0);
				// Tout 1.x, comme le backend : annoncer la version exacte de nos
				// en-tetes a deja fait refuser le runtime Meta.
				li.maxApiVersion = XR_MAKE_VERSION(1, 0xFFFF, 0xFFFFFFFFu);
				XrNegotiateRuntimeRequest rq{};
				rq.structType = XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST;
				rq.structVersion = XR_RUNTIME_INFO_STRUCT_VERSION;
				rq.structSize = sizeof(rq);
				if (XR_SUCCEEDED(negocier(&li, &rq)) && rq.getInstanceProcAddr != nullptr) {
					getProc = rq.getInstanceProcAddr;
				} else {
					printf("        negociation refusee par le runtime\n");
				}
			} else {
				printf("        xrNegotiateLoaderRuntimeInterface absente du runtime\n");
			}
		}
	}
	if (getProc == nullptr) {
		printf("[ECHEC 1] ni loader, ni runtime actif negociable.\n");
		return 1;
	}
	printf("point d'entree OpenXR obtenu depuis : %s\n\n", venuDe);

	PFN_xrCreateInstance createInstance = nullptr;
	getProc(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction *)&createInstance);
	if (createInstance == nullptr) {
		printf("[ECHEC 1] xrCreateInstance non resolue\n");
		return 1;
	}

	XrInstanceCreateInfo ci{};
	ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
	ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	snprintf(ci.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "sonde_openxr_blend");
	ci.applicationInfo.applicationVersion = 1;

	XrInstance instance = XR_NULL_HANDLE;
	XrResult r = createInstance(&ci, &instance);
	if (XR_FAILED(r) || instance == XR_NULL_HANDLE) {
		printf("[ECHEC 2] xrCreateInstance -> %d (aucun runtime OpenXR actif ?)\n", (int)r);
		FreeLibrary(lib);
		return 2;
	}

	PFN_xrGetSystem getSystem = nullptr;
	PFN_xrEnumerateEnvironmentBlendModes enumBlend = nullptr;
	PFN_xrDestroyInstance destroyInstance = nullptr;
	getProc(instance, "xrGetSystem", (PFN_xrVoidFunction *)&getSystem);
	getProc(instance, "xrEnumerateEnvironmentBlendModes", (PFN_xrVoidFunction *)&enumBlend);
	getProc(instance, "xrDestroyInstance", (PFN_xrVoidFunction *)&destroyInstance);

	// Les extensions s'enumerent SANS casque : elles ne dependent que de
	// l'instance. C'est donc le seul fait de cette sonde qui reste etablissable
	// materiel absent -- et il retablit un chiffre dont j'avais dit, a tort,
	// qu'il n'etait pas reproductible.
	PFN_xrEnumerateInstanceExtensionProperties enumExt = nullptr;
	getProc(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction *)&enumExt);
	if (enumExt != nullptr) {
		uint32_t nExt = 0;
		if (XR_SUCCEEDED(enumExt(nullptr, 0, &nExt, nullptr)) && nExt > 0) {
			printf("extensions annoncees par le runtime : %u\n", nExt);
			XrExtensionProperties props[128] = {};
			const uint32_t cap = (nExt < 128) ? nExt : 128;
			for (uint32_t i = 0; i < cap; ++i) {
				props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
			}
			uint32_t ecrit = 0;
			bool passthrough = false;
			if (XR_SUCCEEDED(enumExt(nullptr, cap, &ecrit, props))) {
				for (uint32_t i = 0; i < ecrit; ++i) {
					if (strcmp(props[i].extensionName, "XR_FB_passthrough") == 0) {
						passthrough = true;
					}
				}
			}
			printf("  XR_FB_passthrough : %s\n", passthrough ? "PRESENTE" : "ABSENTE");
			printf("  (le passthrough exige en outre un APK autonome Quest, non produit ici)\n\n");
		}
	}

	int code = 0;
	XrSystemGetInfo sgi{};
	sgi.type = XR_TYPE_SYSTEM_GET_INFO;
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrSystemId systemId = XR_NULL_SYSTEM_ID;

	if (getSystem == nullptr || XR_FAILED(getSystem(instance, &sgi, &systemId))) {
		printf("[ECHEC 2] xrGetSystem : aucun casque presente par le runtime.\n");
		code = 2;
	} else if (enumBlend == nullptr) {
		printf("[ECHEC 3] xrEnumerateEnvironmentBlendModes absente du loader.\n");
		code = 3;
	} else {
		uint32_t n = 0;
		XrEnvironmentBlendMode modes[8] = {};
		r = enumBlend(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 8, &n, modes);
		if (XR_FAILED(r) || n == 0) {
			printf("[ECHEC 3] enumeration -> %d, %u mode(s). INCONNU n'est pas AUCUN.\n", (int)r, n);
			code = 3;
		} else {
			printf("modes annonces : %u\n", n);
			for (uint32_t i = 0; i < n && i < 8; ++i) {
				printf("  %u/%u : %s (valeur %d)\n", i + 1, n, NomMode(modes[i]), (int)modes[i]);
			}

			// ── La regle de NkXrOpenXRBackend, rejouee ────────────────────
			// (copie assumee : lier le backend exigerait le renderer, qui est
			//  precisement ce que cette sonde contourne. Voir l'en-tete.)
			bool opaqueAnnonce = false;
			for (uint32_t i = 0; i < n && i < 8; ++i) {
				if (modes[i] == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) {
					opaqueAnnonce = true;
					break;
				}
			}
			const XrEnvironmentBlendMode choisi = opaqueAnnonce ? XR_ENVIRONMENT_BLEND_MODE_OPAQUE : modes[0];
			printf("\nregle rejouee -> mode soumis : %s\n", NomMode(choisi));
			printf("  (OPAQUE annonce par le runtime : %s)\n", opaqueAnnonce ? "oui" : "NON");

			// ── LE CONTROLE QUI PEUT TOMBER ───────────────────────────────
			bool choisiEstAnnonce = false;
			for (uint32_t i = 0; i < n && i < 8; ++i) {
				if (modes[i] == choisi) {
					choisiEstAnnonce = true;
					break;
				}
			}
			if (!choisiEstAnnonce) {
				printf("\n[ECHEC 4] le mode soumis n'est PAS dans la liste annoncee — "
					   "soumission invalide au regard de la spec.\n");
				code = 4;
			} else {
				printf("\n[OK] le mode soumis est bien dans la liste annoncee.\n");
			}
		}
	}

	if (destroyInstance != nullptr) {
		destroyInstance(instance);
	}
	FreeLibrary(lib);
	printf("code de sortie : %d\n", code);
	return code;
}
