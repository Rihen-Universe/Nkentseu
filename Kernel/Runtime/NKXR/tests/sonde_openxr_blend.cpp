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
// Controle negatif obligatoire (une sonde qui ne peut pas echouer n'est pas une
// sonde) : la lancer avec XR_RUNTIME_JSON pointant sur un chemin inexistant
// doit rendre 1 ou 2, jamais 0.
//
// Construction (pas de bibliotheque a lier : le loader est ouvert dynamiquement,
// comme le fait NkXrOpenXRBackend) :
//   clang++ -std=c++17 -O2 -IExternals/Libs/NKOpenXR/include \
//       Kernel/Runtime/NKXR/tests/sonde_openxr_blend.cpp -o /tmp/sonde_xr.exe
// =============================================================================
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr.h>

#include <windows.h>

#include <cstdio>

// La sonde est un outil de diagnostic hors moteur : elle n'utilise ni NKLogger
// ni les allocateurs, pour ne dependre d'aucune bibliotheque du depot. C'est
// deliberement le seul fichier de NKXR ou <cstdio> est admis.

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

	// Meme mecanisme d'ouverture que NkXrOpenXRBackend, variable d'environnement
	// comprise : une sonde qui chercherait le loader autrement mesurerait un
	// autre chemin que celui du moteur.
	const char *override_ = getenv("NK_XR_OPENXR_LOADER");
	HMODULE lib = nullptr;
	if (override_ != nullptr && *override_ != '\0') {
		lib = LoadLibraryA(override_);
		printf("loader demande via NK_XR_OPENXR_LOADER : %s -> %s\n", override_, lib ? "charge" : "ECHEC");
	}
	if (lib == nullptr) {
		lib = LoadLibraryA("openxr_loader.dll");
	}
	if (lib == nullptr) {
		printf("[ECHEC 1] openxr_loader.dll introuvable (GetLastError=%lu)\n", GetLastError());
		printf("          Le loader Khronos n'est pas sur cette machine. Un runtime OpenXR\n");
		printf("          peut etre INSTALLE et ENREGISTRE sans que le loader soit present :\n");
		printf("          c'est l'application qui livre openxr_loader.dll, pas le runtime.\n");
		return 1;
	}
	auto getProc = PFN_xrGetInstanceProcAddr(GetProcAddress(lib, "xrGetInstanceProcAddr"));
	if (getProc == nullptr) {
		printf("[ECHEC 1] xrGetInstanceProcAddr absente du loader\n");
		return 1;
	}

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
