//
// NkXrConfigEnv.cpp
// =============================================================================
// Description :
//   La couche « variables d'environnement » de NKXR, et RIEN d'autre. Elle
//   pose des valeurs dans une NkXrSessionDesc que l'appelant possède déjà :
//   les réglages restent donc pilotables par le code, l'environnement n'étant
//   qu'un raccourci de développement.
//
// Caractéristiques :
//   - Regroupée ICI, pas dispersée dans les backends : on lit d'un seul coup
//     d'œil tout ce que l'environnement peut changer, et un backend ne peut
//     plus « aller chercher » un réglage en douce derrière le dos de l'app.
//   - Une variable ABSENTE ne touche à rien : la valeur venue du code survit.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/NKIXrBackend.h"

#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace xr {

		namespace {

			bool EnvFloat(const char *name, float32 &target) {
				const char *text = getenv(name);
				if (text == nullptr || *text == '\0') {
					return false;
				}
				target = float32(atof(text));
				return true;
			}

			bool EnvBool(const char *name, bool &target) {
				const char *text = getenv(name);
				if (text == nullptr || *text == '\0') {
					return false;
				}
				// « 0 » ou « false » désarment ; toute autre valeur arme —
				// y compris une variable définie à vide côté shell.
				target = !(text[0] == '0' || strcmp(text, "false") == 0);
				return true;
			}

			// Lit « a,b,c,... » ; rend le nombre de valeurs effectivement lues.
			uint32 EnvFloatList(const char *name, float32 *out, uint32 maxCount) {
				const char *text = getenv(name);
				if (text == nullptr || *text == '\0') {
					return 0;
				}
				uint32 count = 0;
				const char *cursor = text;
				while (count < maxCount) {
					char *end = nullptr;
					const float64 value = strtod(cursor, &end);
					if (end == cursor) {
						break;
					}
					out[count] = float32(value);
					++count;
					cursor = end;
					while (*cursor == ',' || *cursor == ' ') {
						++cursor;
					}
					if (*cursor == '\0') {
						break;
					}
				}
				return count;
			}

		} // namespace

		void NkXrApplyEnvOverrides(NkXrSessionDesc &desc) {
			const char *backend = getenv("NK_XR_BACKEND");
			if (backend != nullptr && strcmp(backend, "openxr") == 0) {
				desc.backend = NkXrBackendType::NK_XR_BACKEND_OPENXR;
			}
			EnvFloat("NK_XR_RENDER_SCALE", desc.renderScale);
			EnvBool("NK_XR_HALF_RATE", desc.halfRate);
			EnvBool("NK_XR_DEPTH_LAYER", desc.submitDepthLayer);
			EnvFloat("NK_XR_HZ", desc.requestedRefreshRateHz);
			EnvFloat("NK_XR_SIM_IPD", desc.ipdMeters);
			EnvBool("NK_XR_LIST_EXT", desc.listExtensions);
			const char *loader = getenv("NK_XR_OPENXR_LOADER");
			if (loader != nullptr && *loader != '\0') {
				desc.openXrLoaderPath = loader;
			}

			// ── Simulateur ───────────────────────────────────────────────────
			float32 pose[5]{};
			if (EnvFloatList("NK_XR_SIM_POSE", pose, 5u) == 5u) {
				desc.simulator.poseFixed = true;
				desc.simulator.yawDegrees = pose[0];
				desc.simulator.pitchDegrees = pose[1];
				desc.simulator.position = NkVec3f(pose[2], pose[3], pose[4]);
			}
			float32 fovDeg[4]{};
			if (EnvFloatList("NK_XR_SIM_FOV", fovDeg, 4u) == 4u) {
				desc.simulator.fovOverride = true;
				const float32 toRad = math::NK_PI_F / 180.f;
				desc.simulator.fov.angleLeft = fovDeg[0] * toRad;
				desc.simulator.fov.angleRight = fovDeg[1] * toRad;
				desc.simulator.fov.angleUp = fovDeg[2] * toRad;
				desc.simulator.fov.angleDown = fovDeg[3] * toRad;
			}
			EnvFloat("NK_XR_SIM_EYE_HEIGHT", desc.simulator.eyeHeightMeters);
			EnvFloat("NK_XR_SIM_SPEED", desc.simulator.moveSpeed);
			EnvFloat("NK_XR_SIM_HZ", desc.simulator.displayHz);
			float32 latencyMs = desc.simulator.latencySeconds * 1000.f;
			if (EnvFloat("NK_XR_SIM_LATENCY_MS", latencyMs)) {
				desc.simulator.latencySeconds = latencyMs * 0.001f;
			}
		}

	} // namespace xr
} // namespace nkentseu
