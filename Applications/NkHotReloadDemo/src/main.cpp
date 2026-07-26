// =============================================================================
// Applications/NkHotReloadDemo/src/main.cpp
// =============================================================================
// Jalon G2.3 (Engine/Noge/ROADMAP.md, « Quintette de scripting » pilier 1) :
// scripting C++ natif RECHARGEABLE À CHAUD, prouvé par exécution réelle.
//
// Cycle complet vérifié par assertions :
//   1. compile scripts/HotCounterScript_v1.cpp (+1/tick) en HotCounterScript.dll
//      via un appel compilateur DIRECT (clang++ msys64/ucrt64, le même que le
//      toolchain jenga) — compilation à RUNTIME, voie la plus robuste : la démo
//      reste un seul exe, aucune cible DLL à orchestrer dans le workspace ;
//   2. NkScriptLoader::LoadDLL (shadow copy Windows + hooks NKMemory injectés),
//      AttachDLLScript sur une entité ECS, 5 ticks NkScriptSystem -> counter=5 ;
//   3. recompile scripts/HotCounterScript_v2.cpp (+2/tick, version 2.0.0)
//      PAR-DESSUS la même .dll (possible grâce à la shadow copy) ;
//   4. NkScriptLoader::HotReload(world) : détection mtime -> CaptureState
//      (Serialize) -> déchargement -> rechargement -> RestoreState ;
//   5. assertions : counter TOUJOURS à 5 après reload (état préservé), version
//      2.0.0 active, 3 ticks -> counter=11 (5 + 3*2 : nouveau comportement,
//      PAS 8 qui serait l'ancien +1).
//
// Zéro STL : NkString/NkVector (NKContainers), NkSharedPtr (NKMemory), libc.
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scripting/NkScriptBridge.h"
#include "Noge/ECS/Scripting/NkScriptSystem.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
// Sleep sans tirer <windows.h> (pollution macro) — signature Win32 officielle.
extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long ms);

static void SleepMs(unsigned ms) {
	::Sleep(ms);
}
#else
#include <unistd.h>

static void SleepMs(unsigned ms) {
	::usleep(ms * 1000u);
}
#endif

using namespace nkentseu;
using namespace nkentseu::ecs;

namespace {

	int gChecks = 0;

#define NK_DEMO_CHECK(cond, msg)                                                                                       \
	do {                                                                                                               \
		if (cond) {                                                                                                    \
			++gChecks;                                                                                                 \
			logger.Info("  CHECK {0} OK : {1}", gChecks, msg);                                                         \
		} else {                                                                                                       \
			logger.Error("  CHECK RATE : {0}", msg);                                                                   \
			logger.Error("=== NkHotReloadDemo : ECHEC ===");                                                           \
			return 1;                                                                                                  \
		}                                                                                                              \
	} while (0)

	// -------------------------------------------------------------------------
	// Localisation de la racine du workspace (pour scripts/ et l'include ABI).
	// Ordre : env NKENTSEU_ROOT, puis remontée depuis le cwd (jusqu'à 8 niveaux)
	// en cherchant le marqueur Engine/Noge/src/Noge/ECS/Scripting/NkScriptABI.h.
	// -------------------------------------------------------------------------
	bool FindWorkspaceRoot(char *out, size_t cap) {
		const char *kMarker = "Engine/Noge/src/Noge/ECS/Scripting/NkScriptABI.h";
		char probe[1024];

		const char *env = ::getenv("NKENTSEU_ROOT");
		if (env && env[0]) {
			std::snprintf(probe, sizeof(probe), "%s/%s", env, kMarker);
			if (NkFile::Exists(probe)) {
				std::snprintf(out, cap, "%s", env);
				return true;
			}
		}

		NkPath cwd = NkDirectory::GetCurrentDirectory();
		char base[1024];
		std::snprintf(base, sizeof(base), "%s", cwd.CStr());
		// Normalise les antislashs
		for (char *p = base; *p; ++p) {
			if (*p == '\\') {
				*p = '/';
			}
		}
		for (int depth = 0; depth < 8; ++depth) {
			std::snprintf(probe, sizeof(probe), "%s/%s", base, kMarker);
			if (NkFile::Exists(probe)) {
				std::snprintf(out, cap, "%s", base);
				return true;
			}
			char *slash = std::strrchr(base, '/');
			if (!slash || slash == base) {
				break;
			}
			*slash = '\0';
		}
		return false;
	}

	// -------------------------------------------------------------------------
	// Localisation du compilateur C++ (le clang-mingw du toolchain jenga).
	// Ordre : env NK_CXX, chemins msys64 connus, puis "clang++" du PATH.
	// -------------------------------------------------------------------------
	const char *FindCompiler(char *out, size_t cap) {
		const char *env = ::getenv("NK_CXX");
		if (env && env[0]) {
			std::snprintf(out, cap, "%s", env);
			return out;
		}
#if defined(_WIN32)
		const char *candidates[] = {
			"C:/msys64/ucrt64/bin/clang++.exe",
			"C:/msys64/mingw64/bin/clang++.exe",
			"C:/msys64/ucrt64/bin/g++.exe",
		};
		for (const char *c : candidates) {
			if (NkFile::Exists(c)) {
				std::snprintf(out, cap, "%s", c);
				return out;
			}
		}
		std::snprintf(out, cap, "clang++");
#else
		std::snprintf(out, cap, "clang++");
#endif
		return out;
	}

	// -------------------------------------------------------------------------
	// Compile un .cpp de script en .dll via un appel compilateur direct.
	// -static (runtime mingw statique) pour que la .dll ne dépende d'aucune
	// libstdc++/libgcc .dll au chargement ; retente sans -static si besoin.
	// -------------------------------------------------------------------------
	bool CompileScript(const char *cxx, const char *root, const char *srcPath, const char *outDll) {
		NkFile::Delete(outDll); // repart d'un fichier propre (mtime net)

		char inner[2048];
		char cmdline[2304];
		const char *staticFlags[] = {"-static", ""};
		for (const char *sf : staticFlags) {
			std::snprintf(inner, sizeof(inner),
						  "\"%s\" -shared -std=c++17 -O1 %s \"-I%s/Engine/Noge/src\" -o \"%s\" \"%s\"", cxx, sf, root,
						  outDll, srcPath);
#if defined(_WIN32)
			// cmd strippe le premier guillemet d'une commande citée : /S /C "..."
			std::snprintf(cmdline, sizeof(cmdline), "cmd /S /C \"%s\"", inner);
#else
			std::snprintf(cmdline, sizeof(cmdline), "%s", inner);
#endif
			logger.Info("  $ {0}", inner);
			const int rc = ::system(cmdline);
			if (rc == 0 && NkFile::Exists(outDll)) {
				return true;
			}
			logger.Warn("  compilation ratee (rc={0}){1}", rc, sf[0] ? " — nouvel essai sans -static" : "");
		}
		return false;
	}

	// Lit le compteur sérialisé du 1er script DLL attaché à l'entité.
	bool ReadCounter(NkWorld &world, NkEntityId entity, int *outCounter) {
		NkScriptHost *host = world.Get<NkScriptHost>(entity);
		if (!host || host->count == 0) {
			return false;
		}
		auto *adapter = dynamic_cast<NkDLLScriptAdapter *>(host->scripts[0].Get());
		if (!adapter) {
			return false;
		}
		char state[256] = {};
		adapter->CaptureState(state, sizeof(state));
		int v = 0;
		if (std::sscanf(state, "{\"counter\":%d}", &v) != 1) {
			return false;
		}
		*outCounter = v;
		return true;
	}

} // namespace

int main() {
	logger.Info("=== NkHotReloadDemo : scripting C++ natif hot-reload (DLL) — jalon G2.3 ===");

	// -- 0. Localisation workspace + compilateur --------------------------------------
	char root[1024] = {};
	if (!FindWorkspaceRoot(root, sizeof(root))) {
		logger.Error("Racine du workspace introuvable. Lancer depuis la racine Nkentseu, ou poser NKENTSEU_ROOT.");
		return 1;
	}
	char cxx[512] = {};
	FindCompiler(cxx, sizeof(cxx));
	logger.Info("Racine workspace : {0}", root);
	logger.Info("Compilateur      : {0}", cxx);

	char workDir[1100];
	std::snprintf(workDir, sizeof(workDir), "%s/Build/HotReloadDemo", root);
	NkDirectory::CreateRecursive(workDir);

	char dllPath[1200];
	std::snprintf(dllPath, sizeof(dllPath), "%s/HotCounterScript" NKECS_DLL_EXT, workDir);
	char srcV1[1200], srcV2[1200];
	std::snprintf(srcV1, sizeof(srcV1), "%s/Applications/NkHotReloadDemo/scripts/HotCounterScript_v1.cpp", root);
	std::snprintf(srcV2, sizeof(srcV2), "%s/Applications/NkHotReloadDemo/scripts/HotCounterScript_v2.cpp", root);

	// Nettoyage des shadow copies d'une exécution précédente
	{
		NkVector<NkString> leftovers = NkDirectory::GetFiles(workDir, "*.hot*");
		for (usize i = 0; i < leftovers.Size(); ++i) {
			NkFile::Delete(leftovers[i].CStr());
		}
	}

	// -- 1. Compilation du script V1 (+1/tick) en DLL ----------------------------------
	logger.Info("-- Etape 1 : compilation du script v1 (compteur +1/tick) en DLL --");
	NK_DEMO_CHECK(NkFile::Exists(srcV1), "source v1 presente (scripts/HotCounterScript_v1.cpp)");
	NK_DEMO_CHECK(CompileScript(cxx, root, srcV1, dllPath), "script v1 compile en HotCounterScript.dll");

	// -- 2. Chargement + attache ECS + 5 ticks -----------------------------------------
	logger.Info("-- Etape 2 : LoadDLL + attache a une entite ECS + 5 ticks --");
	auto &loader = NkScriptLoader::Global();
	NK_DEMO_CHECK(loader.LoadDLL(dllPath), "NkScriptLoader::LoadDLL reussit");
	{
		const NkScriptDLLInfo *info = loader.FindDLLInfo("HotCounterScript");
		NK_DEMO_CHECK(info != nullptr, "factory 'HotCounterScript' resolue (nkecs_get_factory)");
		NK_DEMO_CHECK(info && std::strcmp(info->version, "1.0.0") == 0, "version chargee = 1.0.0");
	}

	NkWorld world;
	NkScriptSystem scriptSystem;
	NkEntityId entity = world.CreateEntity();
	NkScriptHost &host = world.Add<NkScriptHost>(entity);
	NK_DEMO_CHECK(AttachDLLScript(host, "HotCounterScript"), "AttachDLLScript sur l'entite ECS");

	for (int t = 0; t < 5; ++t) {
		scriptSystem.Execute(world, 0.016f);
	}
	int counter = -1;
	NK_DEMO_CHECK(ReadCounter(world, entity, &counter), "etat serialise lisible (Serialize -> JSON)");
	logger.Info("  compteur apres 5 ticks v1 : {0}", counter);
	NK_DEMO_CHECK(counter == 5, "5 ticks v1 (+1) -> compteur = 5");

	// -- 3. Recompilation V2 par-dessus la MEME .dll -----------------------------------
	logger.Info("-- Etape 3 : recompilation v2 (+2/tick) par-dessus la meme .dll --");
	// GetLastWriteTime est en secondes (epoch) : on garantit un mtime STRICTEMENT
	// different meme si la recompilation est ultra-rapide.
	SleepMs(1600);
	NK_DEMO_CHECK(NkFile::Exists(srcV2), "source v2 presente (scripts/HotCounterScript_v2.cpp)");
	NK_DEMO_CHECK(CompileScript(cxx, root, srcV2, dllPath),
				  "script v2 recompile PAR-DESSUS la dll chargee (grace a la shadow copy)");

	// -- 4. Hot-reload : mtime -> capture -> rechargement -> restauration --------------
	logger.Info("-- Etape 4 : NkScriptLoader::HotReload(world) --");
	const uint32 reloaded = loader.HotReload(world);
	NK_DEMO_CHECK(reloaded == 1, "HotReload detecte le mtime change et recharge exactement 1 DLL");
	{
		const NkScriptDLLInfo *info = loader.FindDLLInfo("HotCounterScript");
		NK_DEMO_CHECK(info && std::strcmp(info->version, "2.0.0") == 0, "version active apres reload = 2.0.0");
	}
	counter = -1;
	NK_DEMO_CHECK(ReadCounter(world, entity, &counter), "etat relisible sur la NOUVELLE instance");
	logger.Info("  compteur juste apres reload : {0}", counter);
	NK_DEMO_CHECK(counter == 5, "ETAT PRESERVE : le compteur vaut toujours 5 (pas remis a zero)");

	// -- 5. 3 ticks v2 : le compteur CONTINUE avec le NOUVEAU comportement -------------
	logger.Info("-- Etape 5 : 3 ticks v2 --");
	for (int t = 0; t < 3; ++t) {
		scriptSystem.Execute(world, 0.016f);
	}
	counter = -1;
	NK_DEMO_CHECK(ReadCounter(world, entity, &counter), "etat serialise relisible apres ticks v2");
	logger.Info("  compteur apres 3 ticks v2 : {0}", counter);
	NK_DEMO_CHECK(counter == 11, "NOUVEAU COMPORTEMENT ACTIF : 5 + 3*(+2) = 11 (et pas 8 = ancien +1)");

	// -- Nettoyage ordonné : instances DLL detruites AVANT le dechargement -------------
	{
		NkScriptHost *h = world.Get<NkScriptHost>(entity);
		if (h) {
			for (uint32 i = 0; i < h->count; ++i) {
				h->scripts[i].Reset(); // factory.Destroy pendant que la DLL est chargee
			}
			h->count = 0;
		}
	}
	loader.UnloadAll();

	logger.Info("=== NkHotReloadDemo : {0} assertions OK, 0 echec — hot-reload C++ natif PROUVE ===", gChecks);
	return 0;
}
