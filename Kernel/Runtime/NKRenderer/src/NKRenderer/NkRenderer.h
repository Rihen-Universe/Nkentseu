#pragma once
// =============================================================================
// NkRenderer.h  — NKRenderer v4.0
// Façade publique principale.
// NKRenderer ne connaît PAS : NkScene, ECS, application.
// Seul lien avec la plateforme : NkSurfaceDesc (depuis NKWindow).
// =============================================================================
#include "Core/NkRendererTypes.h"
#include "Core/NkRendererConfig.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Functional/NkFunction.h"

// Forward declarations — évite de tout inclure
namespace nkentseu {
	namespace renderer {
		class NkRenderGraph;
		class NkTextureLibrary;
		class NkShaderLibrary;
		class NkMeshSystem;
		class NkMaterialSystem;
		class NkRender2D;
		class NkRender3D;
		class NkTextRenderer;
		class NkPostProcessStack;
		class NkOverlayRenderer;
		class NkOffscreenTarget;
		class NkShadowSystem;
		class NkVirtualShadowMaps;
		class NkEnvironmentSystem;
		class NkVFXSystem;
		class NkAnimationSystem;
		class NkSimulationRenderer;
		struct NkOffscreenDesc;
	} // namespace renderer
} // namespace nkentseu

namespace nkentseu {
	namespace renderer {

		// Callback UI applicative enregistré dans la passe Overlay2D du render
		// graph (cmd est DANS une render pass active ciblant la sortie finale).
		// Permet à une app (ex. éditeur Nogee) de soumettre ses draw lists NKUI
		// via un backend RHI (Integrations/NKUI/NkUIRHIBackend) sans que
		// NKRenderer connaisse NKUI. [AJOUT 2026-07-25]
		using NkUIOverlayCallback = NkFunction<void(NkICommandBuffer *)>;

		// =========================================================================
		// NkRenderer — interface pure
		// =========================================================================
		class NkRenderer {
			public:
				virtual ~NkRenderer() = default;

				// ── Fabrique ─────────────────────────────────────────────────────────
				// La swapchain est entierement geree par NkIDevice (cf. NkDeviceFactory::Create
				// qui prend la surface). Le renderer recupere les dimensions via le device.
				static NkRenderer *Create(NkIDevice *device, const NkRendererConfig &cfg);
				static void Destroy(NkRenderer *&renderer);

				// ── Cycle de vie ──────────────────────────────────────────────────────
				virtual bool Initialize() = 0;
				virtual void Shutdown() = 0;
				virtual bool IsValid() const = 0;

				// ── Frame ─────────────────────────────────────────────────────────────
				virtual bool BeginFrame() = 0;
				virtual void EndFrame() = 0;
				virtual void Present() = 0;

				// ── Cap de framerate (pacing haute précision, niveau MOTEUR) ──────────
				// Plafonne le FPS dans Present() via un pacing sleep+spin précis (évite le
				// jitter de dt qui fait saccader/clignoter). fps <= 0 => illimité.
				// Défaut au démarrage = variable d'env NK_FPS_CAP (120 si absente, 0=off).
				// MODIFIABLE À CHAUD (dynamique) : appeler pendant le run pour changer/couper.
				virtual void SetFrameRateCap(float32 fps) = 0;
				virtual float32 GetFrameRateCap() const = 0;

				// ── Resize (appeler depuis NkGraphicsContextResizeEvent) ──────────────
				virtual void OnResize(uint32 width, uint32 height) = 0;

				// ── Sous-systèmes ─────────────────────────────────────────────────────
				virtual NkRenderGraph *GetRenderGraph() = 0;
				virtual NkTextureLibrary *GetTextures() = 0;
				virtual NkShaderLibrary *GetShaders() = 0;
				virtual NkMeshSystem *GetMeshSystem() = 0;
				virtual NkMaterialSystem *GetMaterials() = 0;
				virtual NkRender2D *GetRender2D() = 0;
				virtual NkRender3D *GetRender3D() = 0;
				virtual class NkMaterialCollection *GetMaterialCollection() = 0;
				virtual NkTextRenderer *GetTextRenderer() = 0;
				virtual NkPostProcessStack *GetPostProcess() = 0;
				virtual NkOverlayRenderer *GetOverlay() = 0;
				virtual NkVirtualShadowMaps *GetShadow() = 0;
				// L'ENVIRONNEMENT : ce que la scene voit autour d'elle, et donc ce
				// qui l'eclaire sans lampe. Ciel procedural (trois couleurs) ou
				// image HDRI ; le moteur en tire irradiance et reflets.
				virtual NkEnvironmentSystem *GetEnvironment() = 0;
				virtual NkVFXSystem *GetVFX() = 0;
				virtual NkAnimationSystem *GetAnimation() = 0;
				virtual NkSimulationRenderer *GetSimulation() = 0;

				// ── Targets offscreen ─────────────────────────────────────────────────
				virtual NkOffscreenTarget *CreateOffscreen(const NkOffscreenDesc &desc) = 0;
				virtual void DestroyOffscreen(NkOffscreenTarget *&t) = 0;

				// Redirige la SORTIE FINALE du render graph (normalement la swapchain) vers
				// une texture externe (ex. offscreen d'un viewport editeur sur device PARTAGE).
				// handle null => swapchain (defaut). Reconstruit le graph. Permet de rendre le
				// PIPELINE COMPLET (ombres/eclairage/IBL/tonemap) dans un RT echantillonnable.
				virtual void SetFinalColorTarget(NkTextureHandle target) = 0;

				// Variante « voir + enregistrer » : redirige la cible finale COMME
				// SetFinalColorTarget, ET (mirrorToScreen=true) ajoute une passe
				// MirrorPresent qui recopie la cible vers le swapchain (fullscreen
				// blit, ~1 draw) — la fenetre reste vivante pendant la capture /
				// l'enregistrement video, sans bloquer le rendu.
				virtual void SetFinalColorTargetMirror(NkTextureHandle target, bool mirrorToScreen) {
					(void)mirrorToScreen;
					SetFinalColorTarget(target);
				}

				// Resolution de RENDU independante de la fenetre (export/enregistrement
				// haute qualite : rendre en 4K pendant que la fenetre affiche du 720p
				// via la passe MirrorPresent). (0,0) = suit la fenetre (defaut).
				virtual void SetRenderSizeOverride(uint32 w, uint32 h) {
					(void)w;
					(void)h;
				}

				// Couleur d'effacement de la passe Geometry (le « fond » de la scene
				// quand il n'y a pas de skybox). Demande par les editeurs : un
				// viewport de modelage se regle souvent plus clair ou plus neutre que
				// le fond d'un jeu. Reconstruit le graphe : la couleur est figee dans
				// la description de passe, pas lue a chaque image.
				virtual void SetBackgroundColor(NkVec4f rgba) {
					(void)rgba;
				}

				// ── Overlay UI applicatif ─────────────────────────────────────────────
				// Enregistre un callback exécuté en FIN de passe Overlay2D (après
				// Render2D/OverlayRenderer), dans une render pass active sur la
				// sortie finale. Reconstruit le graph (la passe Overlay2D est créée
				// même sans Render2D si un callback est présent). Callback vide =
				// désenregistrement. [AJOUT 2026-07-25 — câblage NKUI de Nogee]
				virtual void SetUIOverlayCallback(const NkUIOverlayCallback &cb) {
					(void)cb;
				}

				// ── Planar reflections (auto) ─────────────────────────────────────────
				// L'utilisateur enregistre un plan reflechissant ; le renderer fait
				// automatiquement la passe miroir avant la passe Geometry principale,
				// et met a jour le material cible avec le RT du reflet.
				// L'utilisateur n'a PLUS BESOIN de soumettre les drawcalls deux fois.
				virtual class NkPlanarReflectionSystem *GetPlanarReflection() = 0;

				// Phase H.6 : voxel AO system. L'app enregistre les occluders
				// (sol, gros meshes static) via GetVoxelAO()->RegisterOccluder()
				// puis appelle Build() une fois pour bake le voxel grid. Le PBR
				// shader sample automatiquement le grid pour atténuer l'IBL des
				// zones occluses (ex: objets sous le sol qui sont cachés du sky).
				virtual class NkVoxelAOSystem *GetVoxelAO() = 0;

				// ── Configuration dynamique ───────────────────────────────────────────
				virtual void SetVSync(bool enabled) = 0;
				virtual void SetPostConfig(const NkPostConfig &pp) = 0;
				virtual void SetWireframe(bool enabled) = 0;
				// Reconstruit le graphe SI un SetPostConfig a change le jeu de passes
				// (SSAO/bloom/FXAA/TAA actives ou non). BeginFrame le fait tout seul ;
				// mais en MODE PARTAGE (editeur : l'hote possede la frame device et
				// « rejoue ce que BeginFrame ferait » — cf. NkDemo3D/NkViewport3D),
				// BeginFrame ne tourne jamais et le drapeau restait arme pour rien :
				// activer l'occlusion ambiante depuis le panneau ne faisait RIEN
				// (constate par Rihen, 9 aout). A appeler dans le bloc de rejeu,
				// AVANT la soumission de la scene — jamais pendant l'execution du
				// graphe.
				virtual void FlushGraphRebuilds() {
				}

				// ── Sous-systemes runtime (enable/disable a chaud) ────────────────────
				// Active un (ou plusieurs) sous-systeme(s) : alloue et initialise s'il
				// n'existe pas encore. Reconstruit le render graph ensuite.
				// Renvoie true si au moins un sous-systeme a ete (re)cree avec succes,
				// false si tous etaient deja actifs ou si un init a echoue.
				virtual bool EnableSubsystem(NkSubsystemFlags flags) = 0;

				// Desactive un (ou plusieurs) sous-systeme(s) : shutdown et libere.
				// Reconstruit le render graph ensuite. Les dependances inverses sont
				// verifiees : desactiver RENDER2D ferme aussi TEXT/UI/OVERLAY si actifs.
				virtual void DisableSubsystem(NkSubsystemFlags flags) = 0;

				// Vrai si TOUS les flags fournis sont actuellement actifs.
				virtual bool IsSubsystemActive(NkSubsystemFlags flags) const = 0;

				// Etat global des sous-systemes (bitfield).
				virtual NkSubsystemFlags GetActiveSubsystems() const = 0;

				// ── Stats ─────────────────────────────────────────────────────────────
				virtual const NkRendererStats &GetStats() const = 0;
				virtual void ResetStats() = 0;

				// ── Accès bas niveau ──────────────────────────────────────────────────
				virtual NkIDevice *GetDevice() const = 0;
				virtual NkICommandBuffer *GetCmd() const = 0;
				virtual uint32 GetFrameIndex() const = 0;
				virtual uint32 GetWidth() const = 0;
				virtual uint32 GetHeight() const = 0;
				virtual const NkRendererConfig &GetConfig() const = 0;
		};

	} // namespace renderer
} // namespace nkentseu
