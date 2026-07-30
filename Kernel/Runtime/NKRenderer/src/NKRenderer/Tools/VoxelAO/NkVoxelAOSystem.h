#pragma once
// =============================================================================
// NkVoxelAOSystem.h — NKRenderer v5.0 (Tools/VoxelAO/)
//
// Phase H.6 : Voxel-based Ambient Occlusion (UE5 Lumen-light approximé).
//
// Principe :
//   1. L'application enregistre des occluders (AABB world-space) via
//      RegisterOccluder(). Typiquement : le sol, les gros meshes static.
//   2. Build() voxelize les occluders en CPU dans une texture 3D R8_UNORM
//      (resolution kRes³, bounds kBounds).
//   3. Le PBR shader sample uVoxelOpacity (binding=27) et fait du
//      cone-tracing dans l'hémisphère normale pour calculer l'AO long-range.
//   4. AO multiplie l'IBL irradiance/specular dans le PBR shader → les
//      zones occluses par d'autres geometry (ex: sphère sous le sol) sont
//      assombries même si l'IBL ne sait pas qu'elles sont cachées.
//
// Limitation v0 :
//   - Voxelization static seulement (au boot, pas update per-frame)
//   - Bounds + resolution hardcodés pour MVP (à exposer en config plus tard)
//   - AABB-based voxelize (pas mesh-precise) — suffisant pour primitives
//
// -----------------------------------------------------------------------------
// GI À UN REBOND — v1 (2026-07-30)
//
// La même grille porte désormais, en plus de l'opacité, la RADIANCE réémise par
// chaque voxel : c'est l'éclairage indirect (« un rebond »). Structure canonique
// du voxel cone tracing (Crassin) dans une seule texture RGBA :
//     RGB = radiance PRÉMULTIPLIÉE par l'opacité   A = opacité
// Ce format permet au shader d'accumuler radiance et occlusion dans UN SEUL
// parcours front-to-back — l'AO devient un produit dérivé du GI, sans coût
// supplémentaire (cf. Include/NkVoxelAO.glsli).
//
// Chaîne : Build() voxelise opacité + albédo (état gardé en RAM) →
// InjectLighting(lights) calcule, pour chaque voxel occupé, l'éclairage direct
// reçu × albédo, avec visibilité par ray-march dans la grille (c'est ce qui
// donne les ombres portées de l'indirect) → upload.
//
// Pourquoi l'injection est en CPU dans cette v1 : la grille est petite (131 072
// voxels), le format RGBA8 3D est le seul universellement échantillonnable sur
// les 4 backends, et le compute DX11 est limité. Le passage en compute ne
// changera que le CALCUL, pas cette interface ni le shader.
// Le format RGBA8 borne la radiance à [0,1] : `NkVoxelAOConfig::giScale` sert
// d'échelle de quantification (la valeur stockée est radiance/giScale) et DOIT
// rester synchronisée avec NK_VOXEL_GI_SCALE dans Include/NkVoxelAO.glsli.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkVec.h"

namespace nkentseu {
	namespace renderer {

		// AABB occluder en world space. Opacity 0..1 (1 = totalement opaque).
		struct NkVoxelOccluder {
				math::NkVec3f minWorld;
				math::NkVec3f maxWorld;
				float32 opacity; // 1.0 = opaque, 0.5 = semi-transparent
				// Couleur diffuse de la surface, utilisee par l'injection GI : c'est
				// elle qui donne sa teinte a l'eclairage indirect (un mur rouge
				// colore en rouge le sol voisin). 0.5 gris = neutre.
				math::NkVec3f albedo = {0.5f, 0.5f, 0.5f};
		};

		struct NkVoxelAOConfig {
				// Bounds world-space couverts par le voxel grid.
				math::NkVec3f minBounds = {-10.f, -5.f, -10.f};
				math::NkVec3f maxBounds = {10.f, 5.f, 10.f};
				// Résolution : 64×32×64 = 131072 voxels = 128 KB R8.
				uint32 resX = 64;
				uint32 resY = 32;
				uint32 resZ = 64;

				// ── GI à un rebond ───────────────────────────────────────────────
				// Échelle de quantification de la radiance stockée en RGBA8 : la
				// valeur écrite est radiance/giScale. Monter cette valeur si
				// l'indirect sature (bandes plates dans les zones très éclairées),
				// la baisser pour gagner en précision sur une scène sombre.
				// ⚠️ DOIT rester égale à NK_VOXEL_GI_SCALE (Include/NkVoxelAO.glsli).
				float32 giScale = 4.f;
				// Nombre de pas du ray-march de visibilité vers chaque lumière. 0
				// désactive le test : l'indirect devient uniforme (pas d'ombres
				// portées dans le rebond), mais l'injection est ~8× plus rapide.
				uint32 giShadowSteps = 12;
		};

		class NkVoxelAOSystem {
			public:
				NkVoxelAOSystem() = default;
				~NkVoxelAOSystem();

				bool Init(NkIDevice *device, const NkVoxelAOConfig &cfg = {});
				void Shutdown();

				bool IsValid() const {
					return mVoxelTex.IsValid();
				}

				// L'app appelle ça pour chaque occluder static (sol, gros mur,
				// gros obstacle). Doit être suivi de Build() pour voxelize +
				// upload GPU.
				void RegisterOccluder(const NkVoxelOccluder &occ) {
					mOccluders.PushBack(occ);
					mDirty = true;
				}

				// Helper : enregistre depuis une AABB sans préciser opacity (=1).
				void RegisterAABB(const math::NkVec3f &mn, const math::NkVec3f &mx, float32 opacity = 1.f) {
					RegisterOccluder({mn, mx, opacity});
				}

				void Clear() {
					mOccluders.Clear();
					mDirty = true;
				}

				// Voxelize les occluders enregistrés en CPU et upload dans la
				// texture 3D R8_UNORM. À appeler une fois après tous les
				// RegisterOccluder() (typiquement à la fin du setup app).
				// Retourne false si pas d'occluders ou texture invalide.
				bool Build();

				// ── GI à un rebond ───────────────────────────────────────────────
				// Calcule la radiance réémise par chaque voxel occupé sous
				// l'éclairage donné, et l'upload dans la grille. À appeler après
				// Build() (qui fournit opacité + albédo), puis à chaque fois que les
				// lumières changent — c'est ce qui rend l'indirect DYNAMIQUE.
				// Coût mesuré en interne et loggé au premier appel : l'appeler à
				// chaque frame sur une grosse grille coûterait cher, d'où
				// InjectLightingIfDirty() qui ne recalcule que si l'éclairage a
				// réellement bougé.
				bool InjectLighting(const NkVector<NkLightDesc> &lights);

				// Variante à appeler sans crainte chaque frame : ne recalcule que si
				// la liste de lumières diffère de celle du dernier calcul (ou si la
				// géométrie a été re-bakée). Retourne true si un calcul a eu lieu.
				bool InjectLightingIfDirty(const NkVector<NkLightDesc> &lights);

				// Intensité de l'indirect appliquée par les shaders (0 = GI éteint,
				// le rendu retombe sur l'ambiant uniforme + AO seul). Sert de
				// contrôle artistique et de A/B de validation.
				void SetGIIntensity(float32 v) {
					mGIIntensity = v < 0.f ? 0.f : v;
				}

				float32 GetGIIntensity() const {
					return mGIIntensity;
				}

				// Vrai si la grille contient une radiance injectée exploitable.
				bool HasGI() const {
					return mHasGI;
				}

				// Accesseurs GPU pour le pipeline PBR.
				NkTextureHandle GetVoxelTexture() const {
					return mVoxelTex;
				}

				NkSamplerHandle GetVoxelSampler() const {
					return mVoxelSampler;
				}

				const NkVoxelAOConfig &GetConfig() const {
					return mCfg;
				}

			private:
				NkIDevice *mDevice = nullptr;
				NkVoxelAOConfig mCfg;

				NkVector<NkVoxelOccluder> mOccluders;
				bool mDirty = true;

				NkTextureHandle mVoxelTex;	   // RGBA8 : rgb = radiance premult, a = opacite
				NkSamplerHandle mVoxelSampler; // linear clamp pour cone trace

				// ── État conservé en RAM pour l'injection GI ─────────────────────
				// Recalculer la radiance ne doit pas exiger de re-voxeliser : Build()
				// dépose ici l'opacité et l'albédo par voxel, InjectLighting les
				// relit. 4 octets/voxel = 512 Ko pour la grille par défaut.
				NkVector<uint8> mOpacity;	  // 1 octet/voxel
				NkVector<uint8> mAlbedoRGB;	  // 3 octets/voxel
				NkVector<uint8> mUpload;	  // tampon RGBA réutilisé (évite de réallouer)
				NkVector<NkLightDesc> mLastLights; // pour InjectLightingIfDirty
				bool mHasGI = false;
				float32 mGIIntensity = 1.f;

				// Radiance réémise par un voxel sous l'éclairage donné. Isolée pour
				// que le futur portage compute ait une référence CPU à comparer.
				math::NkVec3f ComputeVoxelRadiance(const math::NkVec3f &voxelCenter, const math::NkVec3f &normal,
												   const math::NkVec3f &albedo,
												   const NkVector<NkLightDesc> &lights) const;

				// Normale approchée d'un voxel = opposé du gradient d'opacité (la
				// voxelisation par AABB ne fournit pas de normale de surface).
				math::NkVec3f VoxelGradientNormal(uint32 x, uint32 y, uint32 z) const;

				// Fraction de lumière atteignant `from` depuis la direction `toLight`
				// (1 = dégagé, 0 = bloqué), par ray-march dans la grille d'opacité.
				float32 VisibilityTowards(const math::NkVec3f &from, const math::NkVec3f &toLight,
										  float32 maxDist) const;

				// Opacité au point monde (0 hors grille), en voxel le plus proche.
				float32 OpacityAtWorld(const math::NkVec3f &p) const;

				math::NkVec3f VoxelCenterWorld(uint32 x, uint32 y, uint32 z) const;

				// Helper : convertit une AABB world space en range de voxels
				// (indices entiers inclusive). Retourne false si hors bounds.
				bool WorldToVoxelRange(const math::NkVec3f &mn, const math::NkVec3f &mx, uint32 &vMinX, uint32 &vMinY,
									   uint32 &vMinZ, uint32 &vMaxX, uint32 &vMaxY, uint32 &vMaxZ) const;
		};

	} // namespace renderer
} // namespace nkentseu
