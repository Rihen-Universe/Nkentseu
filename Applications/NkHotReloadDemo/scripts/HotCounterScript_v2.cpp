// =============================================================================
// HotCounterScript_v2.cpp — script DLL de la démo hot-reload (VERSION 2).
//
// MÊME script, comportement MODIFIÉ : incrément de +2 au lieu de +1 (c'est le
// changement de code que le hot-reload doit rendre actif SANS perdre l'état :
// le compteur repart de sa valeur restaurée, pas de zéro).
//
// Le nom de classe est identique à la v1 (même factory.info.name) : c'est la
// clé de correspondance utilisée par NkScriptLoader::ReloadDLL pour remplacer
// les instances. Seule la version (2.0.0) et l'incrément changent.
// =============================================================================
#include "Noge/ECS/Scripting/NkScriptABI.h"
#include <stdio.h>

class HotCounterScript : public nkentseu::ecs::NkScriptDLLBase {
	public:
		void OnStart() noexcept override {
			printf("    [script v2] OnStart (counter=%d — etat restaure, pas remis a zero)\n", mCounter);
			fflush(stdout);
		}

		void OnUpdate(float /*dt*/) noexcept override {
			mCounter += 2; // v2 : incrément de +2 (nouveau comportement)
			printf("    [script v2] tick -> counter=%d (+2)\n", mCounter);
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

NK_EXPORT_DLL_SCRIPT_VERSIONED(HotCounterScript, "2.0.0")
