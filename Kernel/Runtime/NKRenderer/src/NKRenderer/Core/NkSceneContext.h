#pragma once
// =============================================================================
// NkSceneContext.h  — NKRenderer v5.0  (Core/)
//
// Contexte d'une frame de rendu 3D : camera, lumieres, IBL, fog, time.
// Fourni par l'utilisateur a NkRender3D::BeginScene(ctx).
//
// Vit dans Core car c'est l'input contract du renderer 3D, et non un detail
// d'implementation de NkRender3D. Les sous-systemes (Shadow, VFX, Animation)
// peuvent egalement le consommer pour rester synchronises.
// =============================================================================
#include "NkRendererTypes.h"
#include "NkCamera.h"

namespace nkentseu {
	namespace renderer {

		// =====================================================================
		// NkSceneContext
		// =====================================================================
		struct NkSceneContext {
				// ── Camera (par valeur — copie a chaque frame, pas de lifetime issue)
				NkCamera3D camera;

				// ── Planar reflection : viewProj de la cam miroir. Le matériau
				// ReflFloor (et d'autres effets-miroir futurs) utilise cette matrice
				// pour échantillonner le RT à la position monde projetée, à la
				// place d'un screen-space mapping (qui ne marche pas avec up flippé).
				// Identity par défaut = pas de reflet (le shader peut détecter et
				// retomber sur un sample neutre).
				NkMat4f mirrorViewProj = NkMat4f::Identity();

				// ── Lights (CPU-side, copie au moment de BeginScene)
				NkVector<NkLightDesc> lights;

				// ── Environnement / IBL
				NkTexHandle envMap; // skybox cubemap (compat NkTextureLibrary)
				NkIBLHandle ibl;	// jeu prefiltre (irradiance + GGX + BRDF LUT)
				float32 iblIntensity = 1.f;
				float32 ambientIntensity = 0.15f;
				NkVec3f ambientColor = {1.f, 1.f, 1.f};

				// ── Time / Frame
				float32 time = 0.f;
				float32 deltaTime = 0.f;
				uint32 frameIdx = 0;

				// ── Fog
				bool fogEnabled = false;
				NkVec3f fogColor = {0.5f, 0.6f, 0.7f};
				float32 fogDensity = 0.f;
				float32 fogStart = 100.f;
				float32 fogEnd = 1000.f;
				// ── BROUILLARD AU SOL (height fog), anime ────────────────────
				// fogThickness = 0 : le brouillard ne depend que de la distance,
				// exactement comme avant. Au-dela, la nappe est DENSE au niveau
				// de fogHeightBase et s'eclaircit en montant sur cette epaisseur.
				// fogWind la souffle comme une FUMEE : sa densite est modulee par
				// un bruit qui DERIVE, a la maniere des nuages -- le sol et le
				// ciel respirent alors ensemble. Le jour ou la physique du vent
				// existera, c'est elle qui pilotera fogWindDir/fogWindSpeed.
				float32 fogHeightBase = 0.f;
				float32 fogThickness = 0.f;
				float32 fogWind = 0.f;		 // force du souffle (0 = nappe lisse)
				float32 fogWindSpeed = 0.1f; // derive de la nappe

				// ── Debug view mode
				NkViewMode viewMode = NkViewMode::NK_SOLID;
		};

	} // namespace renderer
} // namespace nkentseu
