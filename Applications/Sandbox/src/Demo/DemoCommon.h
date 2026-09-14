// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
		// Choix du dorsal. Formes : --backend=<nom>, --backend <nom>, ou les drapeaux
		// courts -bgl -bvk -bdx11 -bdx12 -bmtl -bsw. Sans argument : OpenGL.
		//
		// ⚠️ UN MOT INCONNU EST REFUSE EN LE NOMMANT (11/09). Version precedente : tout
		// `--backend=` non reconnu retombait EN SILENCE sur OpenGL -- `--backend=DX11`
		// ou `--backend=d3d11` lancait OpenGL, et l'on croyait tester DirectX. C'est la
		// regle deja posee pour NK3DModeler (d4b9a1002) : on refuse, on liste les
		// choix, l'appelant sort en code 2, AVANT d'ouvrir la fenetre.
		// La CASSE est ignoree (« DX11 » == « dx11 ») : qui tape DX11 sait ce qu'il
		// veut. Un AUTRE mot (« d3d11 », « vk ») n'est pas devine : refuse.
		// ⚠️ DEUX VOCABULAIRES POUR LE MEME CHOIX : celui-ci (« sw », drapeaux -b*) et
		// celui de NKEditorKit (NkEditorGfxApiFromName : « software », « auto ») que
		// NK3DModeler emploie. Constate, pas unifie ici : renderdemo ne depend pas du kit.
		// Rend false sur un mot inconnu ; `outBad` recoit alors l'argument fautif et
		// `out` n'est PAS touche.
		inline const char *NkDemoBackendChoices() {
			return "opengl|vulkan|dx11|dx12|metal|sw  (ou -bgl -bvk -bdx11 -bdx12 -bmtl -bsw)";
		}
		inline bool NkDemoBackendFromName(const NkString &mot, NkGraphicsApi &out) {
			char m[16];
			const usize n = mot.Size();
			if (n == 0 || n >= sizeof(m))
				return false;
			for (usize k = 0; k < n; ++k) {
				const char c = mot.CStr()[k];
				m[k] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}
			m[n] = 0;
			struct Entree { const char *nom; NkGraphicsApi api; };
			static const Entree kTable[] = {
				{"opengl", NkGraphicsApi::NK_GFX_API_OPENGL}, {"-bgl", NkGraphicsApi::NK_GFX_API_OPENGL},
				{"vulkan", NkGraphicsApi::NK_GFX_API_VULKAN}, {"-bvk", NkGraphicsApi::NK_GFX_API_VULKAN},
				{"dx11", NkGraphicsApi::NK_GFX_API_DX11},	  {"-bdx11", NkGraphicsApi::NK_GFX_API_DX11},
				{"dx12", NkGraphicsApi::NK_GFX_API_DX12},	  {"-bdx12", NkGraphicsApi::NK_GFX_API_DX12},
				{"metal", NkGraphicsApi::NK_GFX_API_METAL},	  {"-bmtl", NkGraphicsApi::NK_GFX_API_METAL},
				{"sw", NkGraphicsApi::NK_GFX_API_SOFTWARE},	  {"-bsw", NkGraphicsApi::NK_GFX_API_SOFTWARE},
			};
			for (const Entree &e : kTable) {
				const char *a = m, *b = e.nom;
				while (*a && *a == *b) { ++a; ++b; }
				if (*a == 0 && *b == 0) { out = e.api; return true; }
			}
			return false;
		}
		inline bool ParseBackend(const NkVector<NkString> &args, NkGraphicsApi &out, NkString &outBad) {
			NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL;
			for (usize i = 1; i < args.Size(); i++) {
				const NkString &a = args[i];
				NkString mot;
				bool porte = false;
				if (a.StartsWith("--backend=")) {
					mot = a.SubStr(10);
					porte = true;
				} else if (a == "--backend") {
					if (i + 1 >= args.Size()) {
						outBad = "--backend (sans valeur)";
						return false;
					}
					mot = args[++i];
					porte = true;
				} else if (a.StartsWith("-b") && NkDemoBackendFromName(a, api)) {
					continue; // drapeau court reconnu
				}
				if (porte && !NkDemoBackendFromName(mot, api)) {
					outBad = mot.Empty() ? NkString("--backend= (vide)") : mot;
					return false;
				}
			}
			out = api;
			return true;
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
		// ⚠️ DEPUIS LE 2026-09-07, `--demo=<NOM>` EST ACCEPTE — parce qu'UN INDICE
		// N'EST PAS UN NOM, et qu'une fusion l'a prouve le jour meme.
		//
		// Le banc d'ombre s'etait enregistre a un indice dans un arbre ; le meme
		// indice etait deja pris par un autre banc du cote de l'integration. La
		// fusion a garde les DEUX etiquettes dans le meme `switch`. Si l'erreur de
		// compilation n'avait pas morde, **le banc aurait mesure sous les reglages
		// de l'autre** — un faux vert que rien, dans ses chiffres, n'aurait
		// permis de soupconner.
		//
		// Un indice est une POSITION dans une table que d'autres modifient ; un nom
		// appartient a la demo. Une commande de banc ecrite dans un rapport, un
		// carnet ou un message de commit survit des semaines : elle doit citer ce
		// qui ne bouge pas. `outName` porte le nom jusqu'au `main`, le seul a
		// connaitre la table.
		inline int ParseDemo(const NkVector<NkString> &args, int defaultIdx = 0, bool *outMalformed = nullptr,
							 NkString *outOffending = nullptr, NkString *outName = nullptr) {
			auto Refuser = [&](const NkString &a) {
				if (outMalformed)
					*outMalformed = true;
				if (outOffending)
					*outOffending = a;
			};
			// Un nom n'est PAS un refus : on le remonte tel quel. Sans `outName`,
			// l'appelant est un ancien consommateur qui ne sait pas le lire — on
			// retombe alors sur le refus QUI PARLE, jamais sur un defaut muet.
			auto Nommer = [&](const NkString &v) -> bool {
				if (!outName)
					return false;
				*outName = v;
				return true;
			};
			for (size_t i = 1; i < args.Size(); i++) {
				const NkString &a = args[i];
				// Forme 1 : --demo=N  ou  --demo=<nom>
				if (a.StartsWith("--demo=")) {
					const NkString v = a.SubStr(7);
					if (!NkIsAllDigits(v.CStr())) {
						if (!v.Empty() && Nommer(v))
							return defaultIdx; // le main resoudra le nom sur la table
						Refuser(a);
						return defaultIdx;
					}
					return atoi(v.CStr());
				}
				// Forme 2 : --demo N  /  -d N  (ou un nom)
				if (a == "--demo" || a == "-d") {
					if (i + 1 < args.Size() && NkIsAllDigits(args[i + 1].CStr()))
						return atoi(args[i + 1].CStr());
					if (i + 1 < args.Size() && !args[i + 1].Empty() && Nommer(args[i + 1]))
						return defaultIdx;
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
