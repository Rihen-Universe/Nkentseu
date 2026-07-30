// =============================================================================
// HotCounterScript_v1.cpp — script DLL de la démo hot-reload (VERSION 1).
//
// Compteur qui s'incrémente de +1 à chaque OnUpdate et sérialise son état en
// JSON ({"counter":N}) — c'est cet état qui doit SURVIVRE au hot-reload.
//
// Compilé à RUNTIME par NkHotReloadDemo via un appel compilateur direct :
//   clang++ -shared -std=c++17 -I <racine>/Engine/Noge/src -o HotCounterScript.dll HotCounterScript_v1.cpp
// (aucun link avec le moteur : ABI C stable de NkScriptABI.h, header autonome)
//
// IMPORTANT hot-reload : OnStart ne touche PAS au compteur — au rechargement,
// l'état est restauré (Deserialize) AVANT le OnStart de la nouvelle instance.
// =============================================================================
#include "Noge/ECS/Scripting/NkScriptABI.h"
#include <stdio.h>

class HotCounterScript : public nkentseu::ecs::NkScriptDLLBase {
	public:
		void OnStart() noexcept override {
			printf("    [script v1] OnStart (counter=%d)\n", mCounter);
			fflush(stdout);
		}

		void OnUpdate(float /*dt*/) noexcept override {
			mCounter += 1; // v1 : incrément de +1
			printf("    [script v1] tick -> counter=%d (+1)\n", mCounter);
			fflush(stdout);
		}

		void Serialize(char *buf, uint32_t size) const noexcept override {
			if (buf && size) {
				snprintf(buf, size, "{\"counter\":%d}", mCounter);
			}
		}

		void Deserialize(const char *json) noexcept override {
			int v = 0;
			if (json && sscanf(json, "{\"counter\":%d}", &v) == 1) {
				mCounter = v;
			}
		}

	private:
		int mCounter = 0;
};

NK_EXPORT_DLL_SCRIPT_VERSIONED(HotCounterScript, "1.0.0")
