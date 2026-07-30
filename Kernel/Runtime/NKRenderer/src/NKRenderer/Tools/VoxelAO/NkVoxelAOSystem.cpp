// =============================================================================
// NkVoxelAOSystem.cpp — NKRenderer v5.0
// =============================================================================
#include "NkVoxelAOSystem.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h" // NkSqrt / NkCos (pas de <math.h> : regle projet)
#include <cstring>
#include <cmath>

namespace nkentseu {
	namespace renderer {

		NkVoxelAOSystem::~NkVoxelAOSystem() {
			Shutdown();
		}

		bool NkVoxelAOSystem::Init(NkIDevice *device, const NkVoxelAOConfig &cfg) {
			mDevice = device;
			mCfg = cfg;
			if (!mDevice)
				return false;

			// Crée la texture 3D RGBA8_UNORM (4 bytes / voxel). R = opacity,
			// G/B/A inutilisés. RGBA8 est universellement supporté en sample 3D
			// sur tous les GPU desktop/mobile (vs R8_UNORM 3D qui requiert
			// VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT specifique). Coût mémoire :
			// 64x32x64*4 = 512KB (acceptable).
			NkTextureDesc td = NkTextureDesc::Tex3D(mCfg.resX, mCfg.resY, mCfg.resZ, NkGPUFormat::NK_RGBA8_UNORM);
			td.debugName = "VoxelAO";
			mVoxelTex = mDevice->CreateTexture(td);
			if (!mVoxelTex.IsValid()) {
				logger.Errorf("[NkVoxelAOSystem] CreateTexture 3D R8 echec (%ux%ux%u)\n", mCfg.resX, mCfg.resY,
							  mCfg.resZ);
				return false;
			}

			// Sampler linear clamp pour cone tracing (interpolation entre voxels).
			mVoxelSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

			// Init clear : remplit le grid avec des zeros (pas d'AO par defaut)
			// afin que le sample retourne 0 et le PBR shader ne soit pas perturbe
			// tant que l'app n'a pas appele Build() avec ses occluders.
			// RGBA8 -> 4 bytes par voxel.
			const uint32 totalVoxels = mCfg.resX * mCfg.resY * mCfg.resZ;
			NkVector<uint8> zeros;
			zeros.Resize(totalVoxels * 4);
			memset(zeros.Data(), 0, totalVoxels * 4);
			mDevice->WriteTexture(mVoxelTex, zeros.Data());

			logger.Info("[NkVoxelAOSystem] Init OK : {0}x{1}x{2} R8 ({3} KB)\n", mCfg.resX, mCfg.resY, mCfg.resZ,
						(mCfg.resX * mCfg.resY * mCfg.resZ) / 1024);
			return true;
		}

		void NkVoxelAOSystem::Shutdown() {
			if (mVoxelSampler.IsValid()) {
				mDevice->DestroySampler(mVoxelSampler);
				mVoxelSampler = {};
			}
			if (mVoxelTex.IsValid()) {
				mDevice->DestroyTexture(mVoxelTex);
				mVoxelTex = {};
			}
			mOccluders.Clear();
			mDirty = true;
		}

		bool NkVoxelAOSystem::WorldToVoxelRange(const math::NkVec3f &mn, const math::NkVec3f &mx, uint32 &vMinX,
												uint32 &vMinY, uint32 &vMinZ, uint32 &vMaxX, uint32 &vMaxY,
												uint32 &vMaxZ) const {
			// Map world AABB -> voxel indices [0, res-1].
			math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
			if (size.x <= 1e-5f || size.y <= 1e-5f || size.z <= 1e-5f)
				return false;

			auto toVoxel = [&](float32 worldComp, float32 minB, float32 sizeB, uint32 res) -> int32 {
				float32 t = (worldComp - minB) / sizeB;
				return (int32)std::floor(t * (float32)res);
			};

			int32 ix0 = toVoxel(mn.x, mCfg.minBounds.x, size.x, mCfg.resX);
			int32 iy0 = toVoxel(mn.y, mCfg.minBounds.y, size.y, mCfg.resY);
			int32 iz0 = toVoxel(mn.z, mCfg.minBounds.z, size.z, mCfg.resZ);
			int32 ix1 = toVoxel(mx.x, mCfg.minBounds.x, size.x, mCfg.resX);
			int32 iy1 = toVoxel(mx.y, mCfg.minBounds.y, size.y, mCfg.resY);
			int32 iz1 = toVoxel(mx.z, mCfg.minBounds.z, size.z, mCfg.resZ);

			// Clamp aux bornes du voxel grid + reject si totalement hors grid.
			if (ix1 < 0 || iy1 < 0 || iz1 < 0)
				return false;
			if (ix0 >= (int32)mCfg.resX || iy0 >= (int32)mCfg.resY || iz0 >= (int32)mCfg.resZ)
				return false;

			vMinX = (uint32)NkMax(0, ix0);
			vMinY = (uint32)NkMax(0, iy0);
			vMinZ = (uint32)NkMax(0, iz0);
			vMaxX = (uint32)NkMin((int32)mCfg.resX - 1, ix1);
			vMaxY = (uint32)NkMin((int32)mCfg.resY - 1, iy1);
			vMaxZ = (uint32)NkMin((int32)mCfg.resZ - 1, iz1);
			return true;
		}

		bool NkVoxelAOSystem::Build() {
			if (!mDevice || !mVoxelTex.IsValid())
				return false;
			// L'opacite et l'albedo sont conserves en RAM : l'injection GI doit
			// pouvoir recalculer la radiance sans re-voxeliser (cf. header).
			const uint32 totalVoxels = mCfg.resX * mCfg.resY * mCfg.resZ;
			mOpacity.Resize(totalVoxels);
			mAlbedoRGB.Resize(totalVoxels * 3);
			mUpload.Resize(totalVoxels * 4);
			memset(mOpacity.Data(), 0, totalVoxels);
			memset(mAlbedoRGB.Data(), 0, totalVoxels * 3);
			memset(mUpload.Data(), 0, totalVoxels * 4);
			mHasGI = false;
			mLastLights.Clear();

			if (mOccluders.Empty()) {
				logger.Warnf("[NkVoxelAOSystem] Build : aucun occluder enregistre\n");
				mDevice->WriteTexture(mVoxelTex, mUpload.Data());
				mDirty = false;
				return true;
			}

			// CPU bake : pour chaque occluder, marque les voxels intersected
			// avec opacity max (un voxel touché par 2 occluders = max des deux).
			// L'albedo suit l'occluder le plus opaque : c'est lui qui domine la
			// couleur du rebond dans ce voxel.
			uint32 voxelsMarked = 0;
			for (uint32 oi = 0; oi < mOccluders.Size(); oi++) {
				const auto &occ = mOccluders[oi];
				uint32 vx0, vy0, vz0, vx1, vy1, vz1;
				if (!WorldToVoxelRange(occ.minWorld, occ.maxWorld, vx0, vy0, vz0, vx1, vy1, vz1))
					continue;

				uint8 op = (uint8)NkClamp(occ.opacity * 255.f, 0.f, 255.f);
				const uint8 ar = (uint8)NkClamp(occ.albedo.x * 255.f, 0.f, 255.f);
				const uint8 ag = (uint8)NkClamp(occ.albedo.y * 255.f, 0.f, 255.f);
				const uint8 ab = (uint8)NkClamp(occ.albedo.z * 255.f, 0.f, 255.f);
				for (uint32 z = vz0; z <= vz1; z++) {
					for (uint32 y = vy0; y <= vy1; y++) {
						uint32 rowStart = (z * mCfg.resY + y) * mCfg.resX;
						for (uint32 x = vx0; x <= vx1; x++) {
							uint32 vi = rowStart + x;
							if (op > mOpacity[vi]) {
								if (mOpacity[vi] == 0)
									voxelsMarked++;
								mOpacity[vi] = op;
								mAlbedoRGB[vi * 3 + 0] = ar;
								mAlbedoRGB[vi * 3 + 1] = ag;
								mAlbedoRGB[vi * 3 + 2] = ab;
							}
						}
					}
				}
			}

			// Upload sans radiance (a = opacite, rgb = 0) : tant qu'aucune lumiere
			// n'a ete injectee, le GI est neutre et seul l'AO opere.
			for (uint32 vi = 0; vi < totalVoxels; vi++)
				mUpload[vi * 4 + 3] = mOpacity[vi];
			mDevice->WriteTexture(mVoxelTex, mUpload.Data());
			mDirty = false;

			logger.Info("[NkVoxelAOSystem] Build OK : {0} occluders -> {1} voxels marques (sur {2})\n",
						mOccluders.Size(), voxelsMarked, totalVoxels);
			return true;
		}

		// =====================================================================
		// GI à un rebond — injection CPU
		// =====================================================================
		math::NkVec3f NkVoxelAOSystem::VoxelCenterWorld(uint32 x, uint32 y, uint32 z) const {
			const math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
			return math::NkVec3f{mCfg.minBounds.x + (((float32)x + 0.5f) / (float32)mCfg.resX) * size.x,
								 mCfg.minBounds.y + (((float32)y + 0.5f) / (float32)mCfg.resY) * size.y,
								 mCfg.minBounds.z + (((float32)z + 0.5f) / (float32)mCfg.resZ) * size.z};
		}

		float32 NkVoxelAOSystem::OpacityAtWorld(const math::NkVec3f &p) const {
			const math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
			if (size.x <= 1e-5f || size.y <= 1e-5f || size.z <= 1e-5f)
				return 0.f;
			const float32 tx = (p.x - mCfg.minBounds.x) / size.x;
			const float32 ty = (p.y - mCfg.minBounds.y) / size.y;
			const float32 tz = (p.z - mCfg.minBounds.z) / size.z;
			if (tx < 0.f || tx >= 1.f || ty < 0.f || ty >= 1.f || tz < 0.f || tz >= 1.f)
				return 0.f;
			const uint32 x = (uint32)(tx * (float32)mCfg.resX);
			const uint32 y = (uint32)(ty * (float32)mCfg.resY);
			const uint32 z = (uint32)(tz * (float32)mCfg.resZ);
			const uint32 vi = (z * mCfg.resY + y) * mCfg.resX + x;
			if (vi >= mOpacity.Size())
				return 0.f;
			return (float32)mOpacity[vi] / 255.f;
		}

		// Normale approchee = -gradient de l'opacite. Une voxelisation par AABB ne
		// porte aucune normale de surface ; le gradient de l'occupation pointe vers
		// l'interieur de la matiere, donc son oppose approche la normale sortante.
		// C'est ce qui permet un terme N.L credible pour l'injection.
		math::NkVec3f NkVoxelAOSystem::VoxelGradientNormal(uint32 x, uint32 y, uint32 z) const {
			auto op = [&](int32 ix, int32 iy, int32 iz) -> float32 {
				if (ix < 0 || iy < 0 || iz < 0)
					return 0.f;
				if (ix >= (int32)mCfg.resX || iy >= (int32)mCfg.resY || iz >= (int32)mCfg.resZ)
					return 0.f;
				const uint32 vi = ((uint32)iz * mCfg.resY + (uint32)iy) * mCfg.resX + (uint32)ix;
				return (float32)mOpacity[vi] / 255.f;
			};
			const int32 ix = (int32)x, iy = (int32)y, iz = (int32)z;
			math::NkVec3f g{op(ix + 1, iy, iz) - op(ix - 1, iy, iz), op(ix, iy + 1, iz) - op(ix, iy - 1, iz),
							op(ix, iy, iz + 1) - op(ix, iy, iz - 1)};
			const float32 len = math::NkSqrt(g.x * g.x + g.y * g.y + g.z * g.z);
			if (len < 1e-4f)
				return math::NkVec3f{0.f, 1.f, 0.f}; // voxel interieur : on suppose vers le haut
			return math::NkVec3f{-g.x / len, -g.y / len, -g.z / len};
		}

		float32 NkVoxelAOSystem::VisibilityTowards(const math::NkVec3f &from, const math::NkVec3f &toLight,
												   float32 maxDist) const {
			const uint32 steps = mCfg.giShadowSteps;
			if (steps == 0)
				return 1.f;
			const math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
			// Pas de l'ordre d'un voxel : plus fin ne change rien a cette resolution,
			// plus grossier laisse passer la lumiere a travers un mur mince.
			const float32 voxel = size.x / (float32)mCfg.resX;
			const float32 stepLen = voxel * 1.25f;
			float32 blocked = 0.f;
			for (uint32 i = 1; i <= steps; i++) {
				const float32 t = stepLen * (float32)i;
				if (t > maxDist)
					break;
				const math::NkVec3f p{from.x + toLight.x * t, from.y + toLight.y * t, from.z + toLight.z * t};
				const float32 o = OpacityAtWorld(p);
				blocked = blocked + o * (1.f - blocked); // Beer's law discret
				if (blocked > 0.99f)
					break;
			}
			return 1.f - NkClamp(blocked, 0.f, 1.f);
		}

		math::NkVec3f NkVoxelAOSystem::ComputeVoxelRadiance(const math::NkVec3f &c, const math::NkVec3f &n,
														   const math::NkVec3f &albedo,
														   const NkVector<NkLightDesc> &lights) const {
			math::NkVec3f irradiance{0.f, 0.f, 0.f};
			for (uint32 li = 0; li < lights.Size(); li++) {
				const NkLightDesc &L = lights[li];
				math::NkVec3f toLight{0.f, 0.f, 0.f};
				float32 atten = 1.f;
				float32 dist = 1e9f;

				if (L.type == NkLightType::NK_DIRECTIONAL) {
					const float32 dl =
						math::NkSqrt(L.direction.x * L.direction.x + L.direction.y * L.direction.y +
									 L.direction.z * L.direction.z);
					if (dl < 1e-5f)
						continue;
					toLight = math::NkVec3f{-L.direction.x / dl, -L.direction.y / dl, -L.direction.z / dl};
				} else {
					math::NkVec3f d{L.position.x - c.x, L.position.y - c.y, L.position.z - c.z};
					dist = math::NkSqrt(d.x * d.x + d.y * d.y + d.z * d.z);
					if (dist < 1e-4f)
						continue;
					toLight = math::NkVec3f{d.x / dist, d.y / dist, d.z / dist};
					// Meme attenuation que le shader PBR : inverse carre borne par
					// la portee, pour que l'indirect suive le direct.
					const float32 range = L.range > 1e-3f ? L.range : 1e-3f;
					const float32 t = NkClamp(1.f - (dist / range), 0.f, 1.f);
					atten = (t * t) / (1.f + dist * dist);
					if (L.type == NkLightType::NK_SPOT) {
						const float32 dl = math::NkSqrt(L.direction.x * L.direction.x +
														L.direction.y * L.direction.y +
														L.direction.z * L.direction.z);
						if (dl < 1e-5f)
							continue;
						// cos entre l'axe du spot et la direction voxel->lumiere
						// inversee : hors du cone exterieur, aucune contribution.
						const float32 cosA = -(toLight.x * (L.direction.x / dl) + toLight.y * (L.direction.y / dl) +
											   toLight.z * (L.direction.z / dl));
						const float32 cosOuter = math::NkCos(L.outerAngle * 3.14159265f / 180.f);
						const float32 cosInner = math::NkCos(L.innerAngle * 3.14159265f / 180.f);
						if (cosA <= cosOuter)
							continue;
						const float32 denom = (cosInner - cosOuter) > 1e-4f ? (cosInner - cosOuter) : 1e-4f;
						atten *= NkClamp((cosA - cosOuter) / denom, 0.f, 1.f);
					}
				}
				if (atten <= 1e-5f)
					continue;

				const float32 ndotl = n.x * toLight.x + n.y * toLight.y + n.z * toLight.z;
				if (ndotl <= 0.f)
					continue;

				// Visibilite : sans elle, l'indirect eclairerait aussi les faces
				// cachees et le GI perdrait tout contraste.
				const math::NkVec3f orig{c.x + n.x * 0.05f, c.y + n.y * 0.05f, c.z + n.z * 0.05f};
				const float32 vis = VisibilityTowards(orig, toLight, dist);
				if (vis <= 1e-4f)
					continue;

				const float32 k = L.intensity * atten * ndotl * vis;
				irradiance.x += L.color.x * k;
				irradiance.y += L.color.y * k;
				irradiance.z += L.color.z * k;
			}

			// Radiance reemise par une surface lambertienne : albedo/pi x irradiance.
			const float32 invPi = 1.f / 3.14159265f;
			return math::NkVec3f{albedo.x * irradiance.x * invPi, albedo.y * irradiance.y * invPi,
								 albedo.z * irradiance.z * invPi};
		}

		bool NkVoxelAOSystem::InjectLighting(const NkVector<NkLightDesc> &lights) {
			if (!mDevice || !mVoxelTex.IsValid())
				return false;
			const uint32 totalVoxels = mCfg.resX * mCfg.resY * mCfg.resZ;
			if (mOpacity.Size() != totalVoxels) {
				logger.Warnf("[NkVoxelAOSystem] InjectLighting sans Build() prealable\n");
				return false;
			}
			if (mUpload.Size() != totalVoxels * 4)
				mUpload.Resize(totalVoxels * 4);

			const float32 scale = mCfg.giScale > 1e-3f ? mCfg.giScale : 1e-3f;
			uint32 lit = 0;
			for (uint32 z = 0; z < mCfg.resZ; z++) {
				for (uint32 y = 0; y < mCfg.resY; y++) {
					for (uint32 x = 0; x < mCfg.resX; x++) {
						const uint32 vi = (z * mCfg.resY + y) * mCfg.resX + x;
						const uint8 op = mOpacity[vi];
						mUpload[vi * 4 + 3] = op;
						if (op == 0) {
							mUpload[vi * 4 + 0] = 0;
							mUpload[vi * 4 + 1] = 0;
							mUpload[vi * 4 + 2] = 0;
							continue;
						}
						const math::NkVec3f albedo{(float32)mAlbedoRGB[vi * 3 + 0] / 255.f,
												   (float32)mAlbedoRGB[vi * 3 + 1] / 255.f,
												   (float32)mAlbedoRGB[vi * 3 + 2] / 255.f};
						const math::NkVec3f c = VoxelCenterWorld(x, y, z);
						const math::NkVec3f n = VoxelGradientNormal(x, y, z);
						math::NkVec3f rad = ComputeVoxelRadiance(c, n, albedo, lights);
						// L'intensite est appliquee ICI et non dans le shader : le
						// PBR n'a alors aucun uniform de GI a recevoir (registres DX
						// contraints), et couper le GI revient a injecter zero.
						rad.x *= mGIIntensity;
						rad.y *= mGIIntensity;
						rad.z *= mGIIntensity;
						// Premultiplication par l'opacite : le shader accumule
						// directement rgb en front-to-back (convention Crassin).
						const float32 a = (float32)op / 255.f;
						const uint8 r = (uint8)NkClamp(rad.x * a / scale * 255.f, 0.f, 255.f);
						const uint8 g = (uint8)NkClamp(rad.y * a / scale * 255.f, 0.f, 255.f);
						const uint8 b = (uint8)NkClamp(rad.z * a / scale * 255.f, 0.f, 255.f);
						mUpload[vi * 4 + 0] = r;
						mUpload[vi * 4 + 1] = g;
						mUpload[vi * 4 + 2] = b;
						if (r || g || b)
							lit++;
					}
				}
			}

			mDevice->WriteTexture(mVoxelTex, mUpload.Data());
			mHasGI = true;
			mLastLights = lights;

			static int sLogged = 0;
			if (sLogged++ == 0)
				logger.Info("[NkVoxelAOSystem] GI inject : {0} lumieres -> {1} voxels emetteurs (scale={2}, "
							"shadowSteps={3})\n",
							lights.Size(), lit, mCfg.giScale, mCfg.giShadowSteps);
			return true;
		}

		bool NkVoxelAOSystem::InjectLightingIfDirty(const NkVector<NkLightDesc> &lights) {
			// Comparaison sur ce qui influe REELLEMENT sur l'injection : type,
			// transform, couleur, intensite, portee, cone. Le reste (ombres, cookie)
			// ne change pas la radiance calculee ici.
			bool same = mHasGI && (mLastLights.Size() == lights.Size());
			if (same) {
				for (uint32 i = 0; i < lights.Size(); i++) {
					const NkLightDesc &a = mLastLights[i];
					const NkLightDesc &b = lights[i];
					auto near3 = [](const math::NkVec3f &u, const math::NkVec3f &v) -> bool {
						const float32 e = 1e-4f;
						return (u.x - v.x) * (u.x - v.x) + (u.y - v.y) * (u.y - v.y) + (u.z - v.z) * (u.z - v.z) <
							   e * e;
					};
					auto near1 = [](float32 u, float32 v) -> bool {
						const float32 d = u - v;
						return (d < 0.f ? -d : d) < 1e-4f;
					};
					if (a.type != b.type || !near3(a.position, b.position) || !near3(a.direction, b.direction) ||
						!near3(a.color, b.color) || !near1(a.intensity, b.intensity) || !near1(a.range, b.range) ||
						!near1(a.innerAngle, b.innerAngle) || !near1(a.outerAngle, b.outerAngle)) {
						same = false;
						break;
					}
				}
			}
			if (same)
				return false;
			return InjectLighting(lights);
		}

	} // namespace renderer
} // namespace nkentseu
