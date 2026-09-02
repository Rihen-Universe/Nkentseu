#pragma once
// =============================================================================
// DemoCommon.h  — Shared utilities for the renderdemo entry point.
//
// Une demo expose deux fonctions :
//   void DemoXxx_Init  (DemoCtx& ctx);
//   void DemoXxx_Frame (DemoCtx& ctx, float32 dt);
//   void DemoXxx_Shutdown(DemoCtx& ctx);
//
// Le main.cpp s'occupe de creer fenetre/device/renderer puis appelle la demo
// selectionnee via --demo=N.
// =============================================================================
#include "../NkRenderer.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"
#include <cstdio>

namespace nkentseu {
	namespace demo {

		using namespace nkentseu;
		using namespace nkentseu::math;
		using namespace nkentseu::renderer;

		// =========================================================================
		// Contexte partage entre main.cpp et les demos individuelles.
		// =========================================================================
		struct DemoCtx {
				NkIDevice *device = nullptr;
				NkRenderer *renderer = nullptr;
				NkWindow *window = nullptr;
				NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL;

				uint32 width = 1280;
				uint32 height = 720;
				float32 totalTime = 0.f;
				uint32 frame = 0;

				// Etat utilisateur (la demo y stocke ses ressources)
				void *userData = nullptr;
		};

		// =========================================================================
		// Helpers : parsing arguments
		// =========================================================================
		inline NkGraphicsApi ParseBackend(const NkVector<NkString> &args) {
			for (size_t i = 1; i < args.Size(); i++) {
				if (args[i] == "--backend=vulkan" || args[i] == "-bvk")
					return NkGraphicsApi::NK_GFX_API_VULKAN;
				if (args[i] == "--backend=dx11" || args[i] == "-bdx11")
					return NkGraphicsApi::NK_GFX_API_DX11;
				if (args[i] == "--backend=dx12" || args[i] == "-bdx12")
					return NkGraphicsApi::NK_GFX_API_DX12;
				if (args[i] == "--backend=metal" || args[i] == "-bmtl")
					return NkGraphicsApi::NK_GFX_API_METAL;
				if (args[i] == "--backend=sw" || args[i] == "-bsw")
					return NkGraphicsApi::NK_GFX_API_SOFTWARE;
			}
			return NkGraphicsApi::NK_GFX_API_OPENGL;
		}

		// Vrai si la chaine est un entier decimal non vide (pas de "2abc", pas de "").
		inline bool NkIsAllDigits(const char *s) {
			if (!s || !*s)
				return false;
			for (const char *c = s; *c; ++c)
				if (*c < '0' || *c > '9')
					return false;
			return true;
		}

		// Selection de la demo. Formes acceptees : --demo=N, --demo N, -d N.
		//
		// ⚠️ POURQUOI CE N'EST PLUS UN SIMPLE StartsWith("--demo="), mesure du
		// 2026-09-02 : l'ancienne version ne reconnaissait QUE la forme a signe
		// egal et RENDAIT LE DEFAUT EN SILENCE pour tout le reste. Taper
		// `renderdemo --demo 2` ou `renderdemo demo 2` lancait la demo 0 sans un
		// mot -- l'utilisateur croit demander la 3D et reçoit autre chose.
		// *Un argument que le programme ne comprend pas doit se dire, jamais se
		// taire.* `outMalformed` porte ce refus jusqu'a l'appelant, qui sort.
		inline int ParseDemo(const NkVector<NkString> &args, int defaultIdx = 0, bool *outMalformed = nullptr,
							 NkString *outOffending = nullptr) {
			auto Refuser = [&](const NkString &a) {
				if (outMalformed)
					*outMalformed = true;
				if (outOffending)
					*outOffending = a;
			};
			for (size_t i = 1; i < args.Size(); i++) {
				const NkString &a = args[i];
				// Forme 1 : --demo=N
				if (a.StartsWith("--demo=")) {
					const NkString v = a.SubStr(7);
					if (!NkIsAllDigits(v.CStr())) {
						Refuser(a);
						return defaultIdx;
					}
					return atoi(v.CStr());
				}
				// Forme 2 : --demo N  /  -d N (valeur dans l'argument suivant)
				if (a == "--demo" || a == "-d") {
					if (i + 1 < args.Size() && NkIsAllDigits(args[i + 1].CStr()))
						return atoi(args[i + 1].CStr());
					Refuser(a);
					return defaultIdx;
				}
				// Forme 3 : `demo 2` nu -- frequent, et jusqu'ici ignore en
				// silence. On le refuse EN LE NOMMANT plutot que de deviner.
				if (a == "demo" || a.StartsWith("--demo")) {
					Refuser(a);
					return defaultIdx;
				}
			}
			return defaultIdx;
		}

		// =========================================================================
		// Format du nom de la demo (utilise par le main pour les logs)
		// =========================================================================
		inline const char *SubsystemFlagsToString(NkSubsystemFlags f, char *buf, size_t n) {
			// ecrit jusqu'a n octets une representation texte abregee
			size_t off = 0;
			auto Add = [&](const char *s) {
				while (*s && off + 1 < n)
					buf[off++] = *s++;
			};
			bool first = true;
			auto Sep = [&]() {
				if (!first)
					Add("|");
				first = false;
			};
			if (NkHasFlag(f, NK_SS_RENDER2D)) {
				Sep();
				Add("R2D");
			}
			if (NkHasFlag(f, NK_SS_RENDER3D)) {
				Sep();
				Add("R3D");
			}
			if (NkHasFlag(f, NK_SS_TEXT)) {
				Sep();
				Add("TEXT");
			}
			if (NkHasFlag(f, NK_SS_UI)) {
				Sep();
				Add("UI");
			}
			if (NkHasFlag(f, NK_SS_SHADOW)) {
				Sep();
				Add("SHADOW");
			}
			if (NkHasFlag(f, NK_SS_POST_PROCESS)) {
				Sep();
				Add("PP");
			}
			if (NkHasFlag(f, NK_SS_VFX)) {
				Sep();
				Add("VFX");
			}
			if (NkHasFlag(f, NK_SS_ANIMATION)) {
				Sep();
				Add("ANIM");
			}
			if (NkHasFlag(f, NK_SS_OVERLAY)) {
				Sep();
				Add("OVERLAY");
			}
			if (NkHasFlag(f, NK_SS_SIMULATION)) {
				Sep();
				Add("SIM");
			}
			if (off == 0)
				Add("NONE");
			buf[off < n ? off : n - 1] = 0;
			return buf;
		}

		// =========================================================================
		// Interface d'une demo
		// =========================================================================
		using DemoInitFn = bool (*)(DemoCtx &);
		using DemoFrameFn = void (*)(DemoCtx &, float32);
		using DemoShutdownFn = void (*)(DemoCtx &);

		struct DemoEntry {
				const char *name;
				const char *description;
				DemoInitFn init;
				DemoFrameFn frame;
				DemoShutdownFn shutdown;
		};

	} // namespace demo
} // namespace nkentseu
