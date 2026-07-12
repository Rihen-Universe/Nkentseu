// =============================================================================
// main.cpp  — Renderdemo entry point (NkRenderer v5.0)
//
// Usage :
//   renderdemo                       # demo par defaut (Demo 0 — Subsystems)
//   renderdemo --demo=N              # selectionne la demo (0=Subsystems, 1=2D, 2=3D, 3=Materials)
//   renderdemo --backend=opengl      # (par defaut)
//   renderdemo --backend=vulkan|dx11|dx12|metal|sw
//
// Liste des demos :
//   0 — Subsystems  : enable/disable runtime des sous-systemes
//   1 — 2D          : sprites + formes + texte (config For2D)
//   2 — 3D          : sphere grid + lights + ombres (config ForGame)
//   3 — Materials   : 5 spheres NkMaterial (PBR/Toon/Anime/Unlit), edition temps reel
//   12 — GLTF       : charge + affiche un modele glTF (rubber_duck, PBR + baseColor)
// =============================================================================
#include "DemoCommon.h"
#include <cstdlib> // getenv (diag opt-in NK_VK_VALIDATION)

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h" // NK_CAPTURE (validation headless)
#include "NKRenderer/Tools/Offscreen/NkFrameCapture.h"	  // NK_RECORD (capture async -> video)
#include "NKMedia/Video/NkVideoRecorder.h"
#include "NKThreading/NkThread.h" // NK_RECORD : finalisation MP4 asynchrone (fix freeze F9)
#include "NKMemory/NkAllocator.h" // NK_RECORD : recorder alloue (possede par le thread de finalisation)				  // NK_RECORD (encodage MP4/H.264 threade)

namespace nkentseu {
	struct NkEntryState;
}

using namespace nkentseu;
using namespace nkentseu::demo;

namespace nkentseu {
	namespace demo {

		// Forward declarations des demos
		bool DemoSubsystems_Init(DemoCtx &);
		void DemoSubsystems_Frame(DemoCtx &, float32);
		void DemoSubsystems_Shutdown(DemoCtx &);
		bool Demo2D_Init(DemoCtx &);
		void Demo2D_Frame(DemoCtx &, float32);
		void Demo2D_Shutdown(DemoCtx &);
		bool Demo3D_Init(DemoCtx &);
		void Demo3D_Frame(DemoCtx &, float32);
		void Demo3D_Shutdown(DemoCtx &);
		bool Demo4_Materials_Init(DemoCtx &);
		void Demo4_Materials_Frame(DemoCtx &, float32);
		void Demo4_Materials_Shutdown(DemoCtx &);
		bool Demo5_Materials_Init(DemoCtx &);
		void Demo5_Materials_Frame(DemoCtx &, float32);
		void Demo5_Materials_Shutdown(DemoCtx &);
		bool Demo6_HierarchicalMaterials_Init(DemoCtx &);
		void Demo6_HierarchicalMaterials_Frame(DemoCtx &, float32);
		void Demo6_HierarchicalMaterials_Shutdown(DemoCtx &);
		bool Demo7_MaterialFunctions_Init(DemoCtx &);
		void Demo7_MaterialFunctions_Frame(DemoCtx &, float32);
		void Demo7_MaterialFunctions_Shutdown(DemoCtx &);
		bool Demo9_Glow2D_Init(DemoCtx &);
		void Demo9_Glow2D_Frame(DemoCtx &, float32);
		void Demo9_Glow2D_Shutdown(DemoCtx &);
		bool Demo8_LayeredV1_Init(DemoCtx &);
		void Demo8_LayeredV1_Frame(DemoCtx &, float32);
		void Demo8_LayeredV1_Shutdown(DemoCtx &);
		bool Demo11_FPSArena_Init(DemoCtx &);
		void Demo11_FPSArena_Frame(DemoCtx &, float32);
		void Demo11_FPSArena_Shutdown(DemoCtx &);
		bool DemoGLTF_Init(DemoCtx &);
		void DemoGLTF_Frame(DemoCtx &, float32);
		void DemoGLTF_Shutdown(DemoCtx &);
		bool DemoSkin_Init(DemoCtx &);
		void DemoSkin_Frame(DemoCtx &, float32);
		void DemoSkin_Shutdown(DemoCtx &);
		bool DemoIK_Init(DemoCtx &);
		void DemoIK_Frame(DemoCtx &, float32);
		void DemoIK_Shutdown(DemoCtx &);
		bool DemoIKChar_Init(DemoCtx &);
		void DemoIKChar_Frame(DemoCtx &, float32);
		void DemoIKChar_Shutdown(DemoCtx &);
		bool DemoAnim_Init(DemoCtx &);
		void DemoAnim_Frame(DemoCtx &, float32);
		void DemoAnim_Shutdown(DemoCtx &);
		bool DemoAnimIK_Init(DemoCtx &);
		void DemoAnimIK_Frame(DemoCtx &, float32);
		void DemoAnimIK_Shutdown(DemoCtx &);
		bool DemoNKGen_Init(DemoCtx &);
		void DemoNKGen_Frame(DemoCtx &, float32);
		void DemoNKGen_Shutdown(DemoCtx &);

		static const DemoEntry kDemos[] = {
			{"Subsystems", "Runtime enable/disable des sous-systemes", DemoSubsystems_Init, DemoSubsystems_Frame,
			 DemoSubsystems_Shutdown},
			{"2D", "Render2D : sprites + shapes + texte", Demo2D_Init, Demo2D_Frame, Demo2D_Shutdown},
			{"3D", "Render3D : grid PBR + lights + ombres", Demo3D_Init, Demo3D_Frame, Demo3D_Shutdown},
			{"Materials", "NkMaterial : 5 spheres multi-materiau, modifications temps reel", Demo4_Materials_Init,
			 Demo4_Materials_Frame, Demo4_Materials_Shutdown},
			{"Materials5", "NkMaterial v2 : evolutions M.2+ (MPC, blend vcolor, hierarchies, etc.)",
			 Demo5_Materials_Init, Demo5_Materials_Frame, Demo5_Materials_Shutdown},
			{"Materials6", "M.4 Hierarchical Material Instances (parent/enfants avec override + propagation)",
			 Demo6_HierarchicalMaterials_Init, Demo6_HierarchicalMaterials_Frame, Demo6_HierarchicalMaterials_Shutdown},
			{"Materials7", "M.5 Material Functions (shader #include + .glsli builtins)", Demo7_MaterialFunctions_Init,
			 Demo7_MaterialFunctions_Frame, Demo7_MaterialFunctions_Shutdown},
			{"Materials8", "M.1 v1 Material Layering N=8 layers (masks vColor/vUV/const)", Demo8_LayeredV1_Init,
			 Demo8_LayeredV1_Frame, Demo8_LayeredV1_Shutdown},
			{"Materials9", "Phase E Materials 2D v0 (Glow2D sprite, future unifie NkMaterial)", Demo9_Glow2D_Init,
			 Demo9_Glow2D_Frame, Demo9_Glow2D_Shutdown},
			// Phase N v1 : meme scene 5 spheres que Demo4 mais avec newport_loft.hdr
			// (contraste fort interieur-loft <-> fenetres lumineuses) + iblStrength
			// boost pour valider visuellement le cubemap HDR brut + tonemap ACES.
			// Reutilise les callbacks Demo4_Materials_* (config differe en BuildConfig).
			{"PhaseNv1", "Phase N v1 : skybox HDR brut + ACES tonemap (newport_loft, contraste fort)",
			 Demo4_Materials_Init, Demo4_Materials_Frame, Demo4_Materials_Shutdown},
			// Demo11 FPS Arena : scene cubes (sol+murs) + spheres + cylindres,
			// textures procedural REPEAT, camera FPS souris+WASD via NkInputQuery.
			{"FPSArena", "Demo11 : arene FPS style x.jpg (sol+murs cubes texturees, FPS cam)", Demo11_FPSArena_Init,
			 Demo11_FPSArena_Frame, Demo11_FPSArena_Shutdown},
			// DemoGLTF : chargement glTF 2.0 (geometrie + materiaux PBR + textures)
			// via NkGLTFLoader + NkGLTFMaterialBridge, affichage camera orbitale.
			{"GLTF", "DemoGLTF : charge + affiche un modele glTF (rubber_duck, PBR + baseColor)", DemoGLTF_Init,
			 DemoGLTF_Frame, DemoGLTF_Shutdown},
			// DemoSkin : skinning GPU (glTF skinne SimpleSkin, pose animee par frame
			// via EvaluateGLTFPose + SubmitSkinned, vertex shader skin LBS).
			{"Skin", "DemoSkin : skinning GPU glTF (SimpleSkin se plie, anime)", DemoSkin_Init, DemoSkin_Frame,
			 DemoSkin_Shutdown},
			// DemoIK : NkAnima M0 — IK FABRIK temps reel (chaine d'os suit une cible animee).
			{"IK", "DemoIK : NkAnima M0 — IK FABRIK (chaine suit une cible, squelette debug)", DemoIK_Init,
			 DemoIK_Frame, DemoIK_Shutdown},
			// DemoIKChar : NkAnima M0 (d) — IK FABRIK sur un VRAI squelette glTF
			// (CesiumMan) : un membre suit une cible, squelette rendu en debug-lines.
			{"IKChar", "DemoIKChar : NkAnima M0 — IK FABRIK sur squelette glTF reel (CesiumMan)", DemoIKChar_Init,
			 DemoIKChar_Frame, DemoIKChar_Shutdown},
			// DemoAnim : NkAnima M1 — pipeline clip + .nkanim binaire + player.
			// glTF -> bake clip -> save .nkanim -> reload -> play -> skinning.
			{"Anim", "DemoAnim : NkAnima M1 — clip + .nkanim binaire + player (CesiumMan rejoue)", DemoAnim_Init,
			 DemoAnim_Frame, DemoAnim_Shutdown},
			// DemoAnimIK : NkAnima M1+M0 — IK PAR-DESSUS l'anim (le corps marche, le bras
			// atteint une cible). Signature Cascadeur : animation + IK ensemble.
			{"AnimIK", "DemoAnimIK : NkAnima M1+M0 — IK sur anim (marche + bras qui atteint une cible)",
			 DemoAnimIK_Init, DemoAnimIK_Frame, DemoAnimIK_Shutdown},
			// DemoNKGen : maillage GENERE par l'IA (NKGen -> Surface Nets) charge et rendu.
			{"NKGen", "DemoNKGen : maillage genere par l'IA (metaballs -> SurfaceNets -> rendu 3D)", DemoNKGen_Init,
			 DemoNKGen_Frame, DemoNKGen_Shutdown},
		};
		static constexpr uint32 kDemoCount = (uint32)(sizeof(kDemos) / sizeof(kDemos[0]));

		static NkRendererConfig BuildConfig(int demoIdx, NkGraphicsApi api, uint32 w, uint32 h) {
			switch (demoIdx) {
				case 0: {
					NkRendererConfig c;
					c.api = api;
					c.width = w;
					c.height = h;
					c.subsystems = NK_SS_RENDER2D | NK_SS_TEXT | NK_SS_OVERLAY;
					c.hdr = false;
					return c;
				}
				case 1:
					return NkRendererConfig::For2D(api, w, h);
				case 2: {
					auto c = NkRendererConfig::ForGame(api, w, h);
					// Demo3D scene tient dans ~7 unites -> 1 cascade large suffit
					// et evite les transitions de cascade qui font scintiller les
					// ombres quand la camera orbite. CSM 4-cascades reste actif
					// dans le code et utilisable pour des scenes plus ouvertes.
					c.shadow.cascadeCount = 1;
					// Test backend software (NK_SW_MINCFG=1) : config 3D minimale, rendu
					// direct swapchain, sans passes offscreen (shadow/post-process) — contourne
					// le crash FBO software non valide et exerce le shader taille NKRenderer.
					if (const char *e = std::getenv("NK_SW_MINCFG"))
						if (e[0] == '1') {
							c.subsystems = NK_SS_RENDER3D;
							c.shadow.cascadeCount = 0;
							c.hdr = false;
						}
					return c;
				}
				case 3: {
					// Demo4 : meme config que Demo3D — 5 spheres, scene ~12 unites.
					// PCSS active (contact-hardening) : sphere ↔ sol -> ombres
					// nettes au contact, plus floues en s'eloignant.
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = true;
					c.voxelAOEnabled = true; // Demo4 enregistre des occluders
					// Phase N v0 : utilise un vrai HDR equirect pour l'IBL au lieu
					// du gradient sky procedural. Visuel beaucoup plus realiste.
					// studio.hdr (256x128) etait trop low-res / peu contraste pour
					// qu'on voie la difference — piazza_bologni_1k.hdr (1024x512)
					// a un ciel bleu + architecture qui apparait visiblement sur
					// les spheres metalliques.
					c.ibl.useHDR = true;
					c.ibl.hdrPath = "Resources/NKRenderer/Textures/Vracs/HDR/piazza_bologni_1k.hdr";
					// Booste iblStrength pour mieux voir l'apport HDR (default 0.3).
					c.ibl.iblStrength = 1.0f;
					// Phase N v0.5 : affiche le HDR comme background skybox visible.
					c.ibl.drawSkybox = true;
					return c;
				}
				case 4: {
					// Demo5 : demo de MATERIAUX (spheres metalliques rough=0.15).
					// Un metal n'a pas de diffus : il ne montre QUE des reflets de
					// l'environnement. Sans environnement visible (drawSkybox=false +
					// IBL faible), les spheres metalliques reflechissent du noir et
					// paraissent sombres ("demo5 sombre"). On lui donne donc un vrai
					// HDR + skybox (comme Demo4) pour que les materiaux soient visibles.
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = false;
					c.ibl.useHDR = true;
					c.ibl.hdrPath = "Resources/NKRenderer/Textures/Vracs/HDR/piazza_bologni_1k.hdr";
					c.ibl.iblStrength = 1.0f;
					c.ibl.drawSkybox = true;
					return c;
				}
				case 5: {
					// Demo6 M.4 : scene legere (4 spheres + 1 petit-enfant + sol).
					// Meme config Game que Demo4/5, 1 cascade suffit, pas de PCSS
					// (les ombres ne sont pas le focus de M.4).
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = false;
					return c;
				}
				case 6: {
					// Demo7 M.5 : 1 sphere custom shader + sol simple. Pas de
					// shadow ni PCSS — la demo est purement procedurale.
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = false;
					return c;
				}
				case 7: {
					// Demo8 M.1 v1 : sphere LayeredV1 + sol. Idem leger.
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = false;
					return c;
				}
				case 8: {
					// Demo9 Phase E Materials 2D : 2D only, pas de shadows ni 3D.
					// Config For2D allege la scene (pas de PBR/IBL/shadow).
					return NkRendererConfig::For2D(api, w, h);
				}
				case 9: {
					// Demo10 Phase N v1 : meme scene 5 spheres que Demo4 (reutilise
					// callbacks Demo4_Materials_*) mais avec un HDR a fort contraste
					// (newport_loft : loft sombre + fenetres tres lumineuses) +
					// iblStrength boost. Objectif : valider visuellement le cubemap
					// skybox dedie HDR brut (RGBA32F, sans Reinhard) + tonemap ACES
					// applique dans le shader skybox. Le contraste rend les bright
					// spots du sun/fenetres bien visibles (vs Reinhard CPU qui les
					// fait disparaitre en gris fade).
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = true;
					c.ibl.useHDR = true;
					c.ibl.hdrPath = "Resources/NKRenderer/Textures/Vracs/HDR/newport_loft.hdr";
					c.ibl.iblStrength = 1.5f; // boost pour amplifier bright spots
					c.ibl.drawSkybox = true;
					return c;
				}
				case 10: {
					// Demo11 FPS Arena : scene 30x30m clos. UNE seule cascade fixe
					// centree origine couvrant toute l'arene. Le mode N=1 dans
					// NkShadowSystem utilise une sphere fixe au lieu de fit-camera
					// -> shadow texels stables, pas de swim/shimmering quand la
					// camera bouge. sceneRadius reglé dans Demo11_Init via
					// GetShadow()->GetConfig().sceneRadius = 25.
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = true;
					c.ibl.useHDR = false; // sky procedural suffit
					c.ibl.drawSkybox = true;
					c.ibl.iblStrength = 0.6f;
					return c;
				}
				case 11: {
					// DemoGLTF : modele glTF unique, scene compacte. Config Game
					// (PBR + IBL sky procedural + ombres). 1 cascade suffit pour un
					// seul objet centre ; pas de PCSS (perf + simplicite).
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = false;
					c.ibl.drawSkybox = true;
					return c;
				}
				case 12: {
					// DemoSkin : modele skinne unique. Config Game minimale, sky
					// procedural visible pour cadrer le mesh, 1 cascade.
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.shadow.pcss = false;
					c.ibl.drawSkybox = true;
					// Bloom OFF : ces modeles sont MATS (metallic=0). Le bloom faisait
					// "briller" les textures saturees (glow neon) alors qu'elles ne
					// refletent pas la lumiere. Sans bloom -> eclairage mat normal.
					c.postProcess.bloom = false;
					c.postProcess.ssr = false;
					// SSAO OFF : dans l'ombre dense sous le modele, le SSAO pousse le
					// sol vers le noir profond ou le tonemap fait apparaitre un voile
					// magenta ("halo" au sol). Un viewer de modele n'a pas besoin de
					// SSAO -> off. (Bug SSAO/tonemap a corriger globalement par ailleurs.)
					c.postProcess.ssao = false;
					// Environnement NEUTRE bien eclaire (look "studio" pour viewer) :
					// gradient gris -> ambient doux INCOLORE qui revele tout le modele
					// (cote ombre visible) sans teinte parasite ni effet miroir.
					c.ibl.skyTop = {0.70f, 0.70f, 0.70f};
					c.ibl.horizon = {0.62f, 0.62f, 0.62f};
					c.ibl.ground = {0.45f, 0.45f, 0.45f};
					c.ibl.iblStrength = 0.40f;
					return c;
				}
				case 13: {
					// DemoIK (NkAnima M0) : squelette IK en sphères PBR. Config Game
					// minimale + IBL neutre clair (ambient de remplissage = scène bien
					// éclairée, sinon tout est noir).
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.postProcess.bloom = false;
					c.postProcess.ssao = false;
					c.postProcess.ssr = false;
					c.ibl.drawSkybox = true; // fond gradient visible (repère)
					c.ibl.skyTop = {0.55f, 0.62f, 0.78f};
					c.ibl.horizon = {0.62f, 0.64f, 0.68f};
					c.ibl.ground = {0.30f, 0.30f, 0.34f};
					c.ibl.iblStrength = 0.85f;
					return c;
				}
				case 15:   // DemoAnim (NkAnima M1) : rejoue .nkanim. Game standard.
				case 16: { // DemoAnimIK (NkAnima M1+M0) : anim + IK. Game standard.
					return NkRendererConfig::ForGame(api, w, h);
				}
				case 14: {
					// DemoIKChar (NkAnima M0 d) : squelette glTF en debug-lines. Même
					// config neutre que DemoIK (Game minimal + IBL clair de remplissage).
					auto c = NkRendererConfig::ForGame(api, w, h);
					c.shadow.cascadeCount = 1;
					c.postProcess.bloom = false;
					c.postProcess.ssao = false;
					c.postProcess.ssr = false;
					c.ibl.drawSkybox = true;
					c.ibl.skyTop = {0.55f, 0.62f, 0.78f};
					c.ibl.horizon = {0.62f, 0.64f, 0.68f};
					c.ibl.ground = {0.30f, 0.30f, 0.34f};
					c.ibl.iblStrength = 0.85f;
					return c;
				}
				default:
					return NkRendererConfig::ForGame(api, w, h);
			}
		}

	} // namespace demo
} // namespace nkentseu

// =============================================================================
// nkmain — entry point unifie
// =============================================================================
int nkmain(const NkEntryState &state) {
	// ── Parse args ───────────────────────────────────────────────────────────
	NkGraphicsApi api = ParseBackend(state.GetArgs());
	int demoIx = ParseDemo(state.GetArgs(), 0);
	// Alias : --demo=N -> index N-1 pour les demos numerotees (Demo4 -> 3, Demo5 -> 4).
	// Coherence avec le nom de fichier plutot que l'index zero-based.
	if (demoIx == 4)
		demoIx = 3;
	if (demoIx == 5)
		demoIx = 4;
	if (demoIx == 6)
		demoIx = 5;
	if (demoIx == 7)
		demoIx = 6;
	if (demoIx == 8)
		demoIx = 7;
	if (demoIx == 9)
		demoIx = 8;
	if (demoIx == 10)
		demoIx = 9; // Demo10 PhaseNv1 -> kDemos[9]
	if (demoIx == 11)
		demoIx = 10; // Demo11 FPSArena -> kDemos[10]
	if (demoIx == 12)
		demoIx = 11; // DemoGLTF       -> kDemos[11]
	if (demoIx == 13)
		demoIx = 12; // DemoSkin       -> kDemos[12]
	if (demoIx == 14)
		demoIx = 13; // DemoIK         -> kDemos[13]
	if (demoIx == 15)
		demoIx = 14; // DemoIKChar     -> kDemos[14]
	if (demoIx == 16)
		demoIx = 15; // DemoAnim       -> kDemos[15]
	if (demoIx == 17)
		demoIx = 16; // DemoAnimIK     -> kDemos[16]
	if (demoIx == 18)
		demoIx = 17; // DemoNKGen      -> kDemos[17]
	if (demoIx < 0 || (uint32)demoIx >= kDemoCount)
		demoIx = 0;
	const DemoEntry &demo = kDemos[demoIx];

	logger.Info("=========================================================\n");
	logger.Info(" NkRenderer v5.0 — Demo {0} ({1}) — Backend : {2}\n", demoIx, demo.name, NkGraphicsApiName(api));
	logger.Info("=========================================================\n");

	// ── Fenetre ──────────────────────────────────────────────────────────────
	NkWindowConfig wcfg;
	wcfg.title = NkFormat("NkRenderer demo : {0}", demo.name);
	wcfg.width = 1280;
	wcfg.height = 720;
	wcfg.centered = true;
	wcfg.resizable = true;

	NkWindow window;
	if (!window.Create(wcfg)) {
		logger.Errorf("[main] Window creation failed\n");
		return 1;
	}

	// ── Device RHI ───────────────────────────────────────────────────────────
	NkSurfaceDesc surface = window.GetSurfaceDesc();
	NkDeviceInitInfo devInfo;
	devInfo.api = api;
	devInfo.surface = surface;
	devInfo.width = (uint32)window.GetSize().width;
	devInfo.height = (uint32)window.GetSize().height;
	devInfo.context.vulkan.appName = "NkRenderer_Demo";
	devInfo.context.vulkan.engineName = "Nkentseu";
	// ── Validation Vulkan (DIAG opt-in, OFF par défaut) ──────────────────────
	// Active VK_LAYER_KHRONOS_validation + debug messenger (route vers NkLogger)
	// UNIQUEMENT si NK_VK_VALIDATION est défini dans l'environnement. Gardé OFF
	// par défaut : la couche peut crasher selon l'env (msvcp140 Huawei DevEco
	// dans le PATH → SIGSEGV vkCreateInstance) et coûte en perf. Sert à diagnostiquer
	// les hazards de synchro (ex. flicker du pipeline skin sur -bvk).
	{
		const char *vkv = getenv("NK_VK_VALIDATION");
		if (vkv && vkv[0] && vkv[0] != '0') {
			devInfo.context.vulkan.validationLayers = true;
			devInfo.context.vulkan.debugMessenger = true;
			logger.Info("[main] Validation Vulkan ACTIVEE (NK_VK_VALIDATION)\n");
		}
	}
	// Format GLOBAL du swapchain (cross-API : GL/VK/DX). UNORM = couleur affichée telle
	// quelle (comme OpenGL/DX) → rendu identique cross-backend. SRGB = encode gamma auto.
	// Une seule ligne pour tous les backends.
	devInfo.context.swapchainFormat = NkSwapchainFormat::NK_SWAPCHAIN_BGRA8_UNORM;

	NkIDevice *device = NkDeviceFactory::Create(devInfo);
	if (!device || !device->IsValid()) {
		logger.Errorf("[main] NkDeviceFactory::Create failed\n");
		window.Close();
		return 2;
	}

	// ── Renderer ─────────────────────────────────────────────────────────────
	uint32 W = (uint32)window.GetSize().width;
	uint32 H = (uint32)window.GetSize().height;
	// PERF backend Software : NK_SW_SCALE (0.3..1.0) réduit la résolution INTERNE de NKRenderer
	// (RT transientes géométrie/tonemap) tout en gardant le swapchain plein écran. La passe
	// finale (FXAA, uv normalisé) upscale automatiquement → moins de pixels = plus de FPS.
	// NOTE PERF : NK_SW_SCALE (réduction des RT intermédiaires) donnait un gain modeste MAIS
	// dédoublait l'overlay 2D (layout texte en pixels fenêtre vs ortho réduit) -> retiré. Le vrai
	// goulot est ailleurs (ex. shadow atlas 4096², cf. NK_SW_SHADOW_SCALE côté device).
	NkRendererConfig cfg = BuildConfig(demoIx, api, W, H);

	char flagsBuf[256];
	SubsystemFlagsToString(cfg.subsystems, flagsBuf, sizeof(flagsBuf));
	logger.Info("[main] Config : {0}x{1}, subsystems = {2}\n", W, H, flagsBuf);

	NkRenderer *renderer = NkRenderer::Create(device, cfg);
	if (!renderer) {
		logger.Errorf("[main] NkRenderer::Create failed (last err : {0})\n", NkRGetLastErrorMessage());
		device->WaitIdle();
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 3;
	}

	// ── Demo init ────────────────────────────────────────────────────────────
	DemoCtx ctx;
	ctx.device = device;
	ctx.renderer = renderer;
	ctx.window = &window;
	ctx.api = api;
	ctx.width = W;
	ctx.height = H;
	if (!demo.init(ctx)) {
		logger.Errorf("[main] Demo init failed\n");
		NkRenderer::Destroy(renderer);
		device->WaitIdle();
		NkDeviceFactory::Destroy(device);
		window.Close();
		return 4;
	}

	// ── Boucle ───────────────────────────────────────────────────────────────
	bool running = true;
	NkClock clock;
	bool recordTogglePending = false; // INSER (declare avant le callback clavier qui le capture)
	NkEventSystem &events = NkEvents();

	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
	events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE)
			running = false;
		// Cap FPS DYNAMIQUE (le pacing est fait par le moteur, cf. NkRenderer::SetFrameRateCap) :
		//   F1 = on/off (bascule cap courant <-> illimité) ; F2 = cycle 30/60/120/144/illimité.
		else if (e->GetKey() == NkKey::NK_INSERT) {
			// Toggle enregistrement video (NK_RECORD a chaud) — traite dans la
			// boucle principale (acces au device/recorder hors du callback).
			// Touche INSER : les F1-F12 sont toutes prises par les demos
			// (conflit constate : F9 = softness ombres / modificateur en demo 2).
			recordTogglePending = true;
		}
		else if (e->GetKey() == NkKey::NK_F1 && renderer) {
			const float32 cur = renderer->GetFrameRateCap();
			static float32 sLast = 120.f;
			if (cur > 0.f) {
				sLast = cur;
				renderer->SetFrameRateCap(0.f);
				logger.Info("[FPS] cap OFF (illimite)\n");
			} else {
				renderer->SetFrameRateCap(sLast);
				logger.Info("[FPS] cap ON = {0}\n", sLast);
			}
		} else if (e->GetKey() == NkKey::NK_F2 && renderer) {
			const float32 steps[] = {30.f, 60.f, 120.f, 144.f, 0.f};
			const float32 cur = renderer->GetFrameRateCap();
			int idx = 0;
			for (int i = 0; i < 5; i++)
				if (steps[i] == cur) {
					idx = i;
					break;
				}
			const float32 next = steps[(idx + 1) % 5];
			renderer->SetFrameRateCap(next);
			logger.Info("[FPS] cap = {0} (0=illimite)\n", next);
		}
	});
	// Resize appliqué IMMÉDIATEMENT dans le handler (et pas différé en boucle de jeu) :
	// sous DX12 flip-model, présenter un swapchain à l'ANCIENNE taille dans une fenêtre déjà
	// redimensionnée (le délai qu'introduisait un debounce) provoque un device removed. En
	// resizant tout de suite, aucun present mismatché ne passe. Le no-op même-taille est géré
	// côté device (ResizeSwapchain) → pas de travail redondant si la taille n'a pas changé.
	events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
		if (w > 0 && h > 0 && (w != ctx.width || h != ctx.height)) {
			ctx.width = w;
			ctx.height = h;
			renderer->OnResize(w, h);
		}
	});

	// NK_MAXFRAMES=<n> : sortie propre apres n frames (vidange du log async pour
	// diag headless). 0/absent = boucle infinie normale.
	const char *mfEnv = getenv("NK_MAXFRAMES");
	const uint64 maxFrames = mfEnv ? (uint64)atoll(mfEnv) : 0;

	// NK_CAPTURE=<frame> : capture du rendu final a la frame donnee vers
	// NK_CAPTURE_PATH (defaut "nk_capture.png"), via SetFinalColorTarget +
	// NkOffscreenTarget readback (marche sur les 6 backends). La fenetre est
	// redirigee 3 frames avant la capture puis restauree. Outil de validation
	// visuelle headless (agents/CI) — 0 cout si NK_CAPTURE absent.
	const char *capEnv = getenv("NK_CAPTURE");
	uint64 captureFrame = capEnv ? (uint64)atoll(capEnv) : 0;
	const char *capPathEnv = getenv("NK_CAPTURE_PATH");
	const char *capturePath = capPathEnv ? capPathEnv : "nk_capture.png";
	renderer::NkOffscreenTarget captureTarget;
	bool captureArmed = false;

	// NK_RECORD=<out.mp4> : enregistre le rendu en video H.264 (NKMedia,
	// encodage sur thread dedie) via la capture ASYNCHRONE NkFrameCapture
	// (ring staging + fences, zero WaitIdle -> le rendu ne stalle jamais).
	// NK_RECORD_FPS=<n> (defaut 10) : cadence d'echantillonnage.
	// ⚠ PLAFOND MESURE (2026-07-12) : l'encodeur H.264 CPU soutient ~10 fps
	// en 720p (RAM plate) ; a 30 fps la file d'encodage NON BORNEE de
	// NkVideoRecorder gonfle de ~100 Mo/s et sature la machine. File bornee
	// + drop policy a demander cote NKMedia (module de l'autre agent).
	// La fenetre RESTE VIVANTE pendant l'enregistrement (passe MirrorPresent
	// du moteur : blit plein-ecran de la cible redirigee vers le swapchain).
	const char *recEnv = getenv("NK_RECORD");
	const char *recFpsEnv = getenv("NK_RECORD_FPS");
	const int32 recordFps = recFpsEnv ? (int32)atoll(recFpsEnv) : 10;
	// NK_RECORD_QP=<10..40> : qualite H.264 (quantization parameter). Plus BAS
	// = plus fin (moins de blocs de compression, fichier plus gros, encodage
	// plus lent). Defaut 24 (celui de NkVideoRecorder) ; 16-18 = tres propre.
	const char *recQpEnv = getenv("NK_RECORD_QP");
	int32 recordQp = recQpEnv ? (int32)atoll(recQpEnv) : 24;
	if (recordQp < 10)
		recordQp = 10;
	if (recordQp > 40)
		recordQp = 40;
	// NK_RECORD_CODEC=mjpeg : encode en MJPEG (intra pur, zero macroblocking
	// inter-trame, cadences hautes OK) au lieu du H.264 (defaut). Qualite via
	// NK_RECORD_MJPEG_Q=<1..100> (defaut 90). MJPEG = video seule (pas d'audio).
	const char *recCodecEnv = getenv("NK_RECORD_CODEC");
	const bool recordMjpeg = recCodecEnv && (recCodecEnv[0] == 'm' || recCodecEnv[0] == 'M');
	const char *recMjqEnv = getenv("NK_RECORD_MJPEG_Q");
	int32 recordMjpegQ = recMjqEnv ? (int32)atoll(recMjqEnv) : 90;
	if (recordMjpegQ < 1)
		recordMjpegQ = 1;
	if (recordMjpegQ > 100)
		recordMjpegQ = 100;
	char recordPath[256] = {0};
	if (recEnv && recEnv[0])
		snprintf(recordPath, sizeof(recordPath), "%s", recEnv);
	bool recording = recordPath[0] != 0;
	uint32 recordSeq = 0;			  // noms auto nk_record_NNN.mp4 (toggle INSER)
	renderer::NkOffscreenTarget recordTarget;
	renderer::NkFrameCapture recordCapture;
	media::NkVideoRecorder *recorder = nullptr; // alloue au demarrage d'un enregistrement
	threading::NkThread recordFinalizeThread;	// End() draine la file d'encodage -> HORS du thread de rendu

	// NK_RECORD_W/H : resolution d'ENREGISTREMENT independante de la fenetre
	// (ex: 3840x2160 pendant un affichage 720p). Le moteur rend a cette taille
	// (SetRenderSizeOverride) et la passe MirrorPresent re-echantillonne vers
	// l'ecran. Qualite NATIVE (pas d'upscale). Defaut 0 = taille fenetre.
	const char *recWEnv = getenv("NK_RECORD_W");
	const char *recHEnv = getenv("NK_RECORD_H");
	uint32 recOutW = recWEnv ? ((uint32)atoll(recWEnv) & ~1u) : 0;
	uint32 recOutH = recHEnv ? ((uint32)atoll(recHEnv) & ~1u) : 0;
	if ((recOutW == 0) != (recOutH == 0)) {
		recOutW = 0;
		recOutH = 0; // les DEUX ou aucun
	}
	float64 recordAccum = 0.0;
	uint64 recordPushed = 0;

	// NK_RECORD_RECT=x,y,w,h : n'enregistre que cette ZONE de l'image (crop
	// CPU au moment de pousser vers l'encodeur ; w/h alignes a 2 pour H.264,
	// clampes a la fenetre). La qualite = resolution du rendu source.
	uint32 recRX = 0, recRY = 0, recRW = 0, recRH = 0;
	bool recHasRect = false;
	if (const char *rr = getenv("NK_RECORD_RECT")) {
		unsigned x = 0, y = 0, w = 0, h = 0;
		if (sscanf(rr, "%u,%u,%u,%u", &x, &y, &w, &h) == 4 && w >= 16 && h >= 16) {
			recRX = x;
			recRY = y;
			recRW = w & ~1u;
			recRH = h & ~1u;
			recHasRect = true;
		}
	}
	NkVector<uint8> recCrop; // scratch du crop (reutilise)

	// Arret PROPRE de l'enregistrement (fin, resize, toggle INSER). Draine les
	// captures en vol (borne), finalise le MP4, restaure le swapchain.
	auto stopRecording = [&]() {
		if (!recordTarget.IsValid())
			return;
		device->WaitIdle();
		for (int guard = 0; recordCapture.PendingCount() > 0 && guard < 64; ++guard) {
			if (!recordCapture.Poll([&](const uint8 *px, uint32 w, uint32 h, uint64) {
					(void)w;
					(void)h;
					if (recorder)
						recorder->PushVideo(px, media::NkVideoInputFormat::RGBA32);
				}))
				break;
		}
		// Restaure l'affichage IMMEDIATEMENT (le rendu continue sans a-coup)...
		renderer->SetFinalColorTarget(NkTextureHandle{});
		if (recOutW)
			renderer->SetRenderSizeOverride(0, 0);
		recordCapture.Shutdown();
		recordTarget.Shutdown();
		if (recorder)
			logger.Infof("[main] NK_RECORD stats : file=%d, droppees=%llu, encodage=%.1f fps\n",
						 recorder->QueueDepth(), (unsigned long long)recorder->DroppedFrames(),
						 recorder->EncodeFps());
		// ... et FINALISE le MP4 sur un THREAD DEDIE : recorder->End() draine
		// toute la file d'encodage (potentiellement des secondes) — sur le
		// thread de rendu ca FIGEAIT l'application (bug F9 constate par Rihen).
		// Le thread prend possession du recorder et le libere.
		if (recorder) {
			if (recordFinalizeThread.Joinable())
				recordFinalizeThread.Join(); // une finalisation precedente encore en cours
			media::NkVideoRecorder *toEnd = recorder;
			recorder = nullptr;
			recordFinalizeThread.Start([toEnd](void *) {
				toEnd->End();
				memory::NkGetDefaultAllocator().Delete(toEnd);
			});
		}
		logger.Infof("[main] NK_RECORD arrete : %llu trames -> %s (finalisation en fond)\n",
					 (unsigned long long)recordPushed, recordPath);
		recording = false;
	};

	// Le cap FPS (garde-fou thermique + anti-jitter) est désormais géré par le MOTEUR
	// dans NkRenderer::Present() (pacing haute précision). Défaut = NK_FPS_CAP (120),
	// modifiable à chaud via F1/F2 (cf. callback clavier). Plus de limiteur ici.
	while (running) {
		events.PollEvents();
		if (!running)
			break;
		if (maxFrames && ctx.frame >= maxFrames) {
			running = false;
			break;
		}

		float32 dt = clock.Tick().delta;
		if (dt <= 0.f || dt > 0.25f)
			dt = 1.f / 60.f;
		ctx.totalTime += dt;
		ctx.frame++;

		if (ctx.width == 0 || ctx.height == 0)
			continue;

		// ── NK_RECORD : capture async -> NkVideoRecorder (thread encode) ────
		// Toggle INSER : demarre (nom auto nk_record_NNN.mp4) / arrete a chaud.
		if (recordTogglePending) {
			recordTogglePending = false;
			if (recording) {
				stopRecording();
			} else {
				snprintf(recordPath, sizeof(recordPath), "nk_record_%03u.mp4", ++recordSeq);
				recordPushed = 0;
				recording = true; // init lazy ci-dessous
			}
		}
		if (recording) {
			// Resize pendant l'enregistrement : tailles cible/capture/encodeur
			// incoherentes -> on STOPPE proprement (protege contre les blocages).
			if (recordTarget.IsValid() && recOutW == 0 &&
				(recordTarget.GetWidth() != ctx.width || recordTarget.GetHeight() != ctx.height)) {
				logger.Warnf("[main] NK_RECORD: resize fenetre detecte, arret de l'enregistrement\n");
				stopRecording();
			} else if (!recordTarget.IsValid()) {
				// Resolution d'ENREGISTREMENT : NK_RECORD_W/H (qualite native,
				// independante de la fenetre) sinon taille de la fenetre.
				const uint32 rw = recOutW ? recOutW : ctx.width;
				const uint32 rh = recOutH ? recOutH : ctx.height;
				renderer::NkOffscreenDesc od;
				od.width = rw;
				od.height = rh;
				od.hasDepth = false;
				od.colorFmt = ::nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
				od.readback = false; // le readback passe par NkFrameCapture
				od.name = "nk_record";
				renderer::NkFrameCaptureDesc fd;
				fd.width = rw;
				fd.height = rh;
				// Zone : clamp a l'image RENDUE (rw x rh).
				uint32 vw = rw, vh = rh;
				if (recHasRect) {
					const uint32 rx = recRX < rw ? recRX : 0;
					const uint32 ry = recRY < rh ? recRY : 0;
					vw = (rx + recRW <= rw) ? recRW : ((rw - rx) & ~1u);
					vh = (ry + recRH <= rh) ? recRH : ((rh - ry) & ~1u);
					recRX = rx;
					recRY = ry;
					recRW = vw;
					recRH = vh;
					recCrop.Resize((usize)vw * vh * 4u);
				}
				if (!recorder)
					recorder = memory::NkGetDefaultAllocator().New<media::NkVideoRecorder>();
				bool ok = vw >= 16 && vh >= 16 && recordTarget.Init(device, renderer->GetTextures(), od) &&
						  recordCapture.Init(device, fd) &&
						  recorder->Begin(recordPath, (int32)vw, (int32)vh, recordFps, 1, recordQp,
										  /*maxQueuedFrames*/ 32,
										  recordMjpeg ? media::NkRecorderCodec::MJPEG : media::NkRecorderCodec::H264,
										  recordMjpegQ);
				if (ok && recOutW)
					renderer->SetRenderSizeOverride(rw, rh); // rendu interne a la taille d'export
				if (ok) {
					renderer->SetFinalColorTargetMirror(
						renderer->GetTextures()->GetRHIHandle(recordTarget.GetColorResult()), /*mirrorToScreen=*/true);
					logger.Infof("[main] NK_RECORD -> %s (%d fps, %ux%u%s)\n", recordPath, recordFps, vw, vh,
								 recHasRect ? " [zone]" : "");
				} else {
					logger.Warnf("[main] NK_RECORD: init KO, enregistrement annule\n");
					recording = false;
				}
			} else {
				// Echantillonnage a recordFps : enqueue la copie de la frame
				// rendue precedente (non bloquant, saute si ring plein).
				recordAccum += (float64)dt;
				const float64 interval = 1.0 / (float64)recordFps;
				if (recordAccum >= interval) {
					recordAccum -= interval;
					// AUTO-REGULATION (stats NKMedia 0bbaabcb) : si la file
					// d'encodage approche son cap, on saute l'echantillon ICI
					// (economise copie GPU + readback + memcpy d'une trame que
					// le recorder dropperait de toute facon).
					const bool encoderBusy = recorder && recorder->QueueDepth() >= 24;
					if (!encoderBusy)
						(void)recordCapture.EnqueueCopy(
							renderer->GetTextures()->GetRHIHandle(recordTarget.GetColorResult()), ctx.frame);
				}
				// Draine les captures pretes vers l'encodeur (deja threade).
				while (recordCapture.Poll([&](const uint8 *rgba, uint32 w, uint32 h, uint64) {
					(void)h;
					if (recHasRect) {
						// Crop CPU de la zone -> scratch (lignes contigues).
						const uint32 rowBytes = recRW * 4u;
						for (uint32 row = 0; row < recRH; ++row)
							memcpy(recCrop.Data() + (usize)row * rowBytes,
								   rgba + ((usize)(recRY + row) * w + recRX) * 4u, rowBytes);
						if (recorder)
							recorder->PushVideo(recCrop.Data(), media::NkVideoInputFormat::RGBA32);
					} else {
						if (recorder)
							recorder->PushVideo(rgba, media::NkVideoInputFormat::RGBA32);
					}
					recordPushed++;
				})) {
				}
			}
		}

		// ── NK_CAPTURE : redirection -> readback -> restauration ────────────
		if (captureFrame > 0) {
			if (!captureArmed && ctx.frame + 3 >= captureFrame) {
				renderer::NkOffscreenDesc od;
				od.width = ctx.width;
				od.height = ctx.height;
				od.hasDepth = false;
				od.colorFmt = ::nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
				od.readback = true;
				od.name = "nk_capture";
				if (captureTarget.Init(device, renderer->GetTextures(), od)) {
					renderer->SetFinalColorTarget(renderer->GetTextures()->GetRHIHandle(captureTarget.GetColorResult()));
					captureArmed = true;
				} else {
					logger.Warnf("[main] NK_CAPTURE: offscreen init KO, capture annulee\n");
					captureFrame = 0;
				}
			} else if (captureArmed && ctx.frame > captureFrame) {
				device->WaitIdle();
				const bool ok = captureTarget.Capture(capturePath);
				logger.Infof("[main] NK_CAPTURE frame %llu -> %s (%s)\n",
							 (unsigned long long)ctx.frame, capturePath, ok ? "OK" : "ECHEC");
				renderer->SetFinalColorTarget(NkTextureHandle{});
				captureTarget.Shutdown();
				captureFrame = 0; // one-shot
			}
		}

		demo.frame(ctx, dt);
	}

	// ── NK_RECORD : drainage final + finalisation MP4 ────────────────────────
	if (recording && recordTarget.IsValid())
		stopRecording();

	// ── Cleanup ──────────────────────────────────────────────────────────────
	demo.shutdown(ctx);
	NkRenderer::Destroy(renderer);
	device->WaitIdle();
	NkDeviceFactory::Destroy(device);
	window.Close();

	// NK_RECORD : si une finalisation MP4 est encore en cours, l'ATTENDRE avant
	// de quitter — sinon le processus meurt avec le thread detache et le fichier
	// reste TRONQUE (l'index MP4 s'ecrit a la fin). La fenetre est deja fermee,
	// le blocage est invisible pour l'utilisateur.
	if (recordFinalizeThread.Joinable()) {
		logger.Info("[main] NK_RECORD : finalisation du MP4 en cours, attente...\n");
		recordFinalizeThread.Join();
	}

	logger.Info("[main] Bye\n");
	return 0;
}
