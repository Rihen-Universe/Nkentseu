// =============================================================================
// NkVoxelAOSystem.cpp — NKRenderer v5.0
// =============================================================================
#include "NkVoxelAOSystem.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h" // NkSqrt / NkCos (pas de <math.h> : regle projet)
#include "NKTime/NkChrono.h"	// mesure du cout de la re-voxelisation / injection
#include "NKMemory/NkFunction.h" // (09/11/2026) memory::NkSet
#include <cstring>
#include <cmath>
#include <cstdlib> // getenv / atof (overrides de diagnostic NK_*)

namespace nkentseu {
	namespace renderer {

		NkVoxelAOSystem::~NkVoxelAOSystem() {
			Shutdown();
		}

		bool NkVoxelAOSystem::Init(NkIDevice *device, const NkVoxelAOConfig &cfg) {
			mDevice = device;
			mCfg = cfg;
			mCfgInit = cfg; // (09/11/2026) ResetBounds y revient
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
			mTriNormal.Clear(); // (09/11/2026) refaite par VoxelizeTriangles s'il y a des triangles

			if (mOccluders.Empty() && mTris.Empty()) {
				// GRILLE VIDE = ETAT LEGITIME (occlusion coupee au panneau, scene
				// nue) : une INFO une seule fois, pas un avertissement par rebuild
				// (le journal de Rihen s'en remplissait, 11 aout).
				static bool sSaidEmpty = false;
				if (!sSaidEmpty) {
					sSaidEmpty = true;
					logger.Info("[NkVoxelAOSystem] Build : aucun occluder (grille vide)\n");
				}
				mDevice->WriteTexture(mVoxelTex, mUpload.Data());
				mDirty = false;
				return true;
			}

			// CPU bake : pour chaque occluder, marque les voxels intersected
			// avec opacity max (un voxel touché par 2 occluders = max des deux).
			// L'albedo suit l'occluder le plus opaque : c'est lui qui domine la
			// couleur du rebond dans ce voxel.
			const NkElapsedTime tBuild0 = NkChrono::Now();
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

			// (09/11/2026) puis les triangles, voxel par voxel traverse.
			if (!mTris.Empty())
				voxelsMarked += VoxelizeTriangles();

			// Upload sans radiance (a = opacite, rgb = 0) : tant qu'aucune lumiere
			// n'a ete injectee, le GI est neutre et seul l'AO opere.
			for (uint32 vi = 0; vi < totalVoxels; vi++)
				mUpload[vi * 4 + 3] = mOpacity[vi];
			if (mFiltre > 0)
				Filtrer(mUpload); // (09/11/2026) opt-in, voir SetFiltre
			mDevice->WriteTexture(mVoxelTex, mUpload.Data());
			mDirty = false;

			mLastBuildMs = (float32)(NkChrono::Now() - tBuild0).ToMilliseconds();
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

			const NkElapsedTime tInj0 = NkChrono::Now();
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
						math::NkVec3f n = VoxelGradientNormal(x, y, z);
						// (09/11/2026) la normale des triangles, si elle existe et ne
						// s'annule pas (un mur mince vu des deux cotes garde le gradient).
						if (mTriNormal.Size() == totalVoxels * 3) {
							const float32 tx = mTriNormal[vi * 3 + 0], ty = mTriNormal[vi * 3 + 1],
										  tz = mTriNormal[vi * 3 + 2];
							const float32 tl = math::NkSqrt(tx * tx + ty * ty + tz * tz);
							if (tl > 0.3f)
								n = math::NkVec3f{tx / tl, ty / tl, tz / tl};
						}
						math::NkVec3f rad = ComputeVoxelRadiance(c, n, albedo, lights);
						// (09/11/2026) le ciel vu par ce voxel (opt-in, SetCielInjecte) :
						// radiance reemise = albedo/pi x (pi x L x part visible) = albedo x L x part.
						if (mCielRayons > 0) {
							const float32 part = CielVisible(c, n);
							rad.x += albedo.x * mCielRad.x * part;
							rad.y += albedo.y * mCielRad.y * part;
							rad.z += albedo.z * mCielRad.z * part;
						}
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

			// Sonde de diagnostic : NK_GI_DEBUG_FILL remplit TOUTE la grille d'une
			// radiance uniforme (et d'une opacité franche). Si le rendu ne bouge
			// pas sous cette sonde, le défaut est dans la LECTURE du shader, pas
			// dans l'injection ni dans la couverture des cônes — c'est le test qui
			// sépare les deux moitiés de la chaîne.
			{
				const char *dbg = getenv("NK_GI_DEBUG_FILL");
				if (dbg && dbg[0] && dbg[0] != '0') {
					const uint8 v = (uint8)NkClamp((float32)atof(dbg) * 255.f, 0.f, 255.f);
					for (uint32 i = 0; i < totalVoxels; i++) {
						mUpload[i * 4 + 0] = v;
						mUpload[i * 4 + 1] = 0;
						mUpload[i * 4 + 2] = 0;
						mUpload[i * 4 + 3] = 0;
					}
					// ⚠ `Warnf` = famille printf : `%u`, pas `{0}` (la valeur était perdue).
					logger.Warnf("[NkVoxelAOSystem] NK_GI_DEBUG_FILL actif : grille saturee R=%u\n", (uint32)v);
				}
			}

			if (mFiltre > 0)
				Filtrer(mUpload); // (09/11/2026) opt-in, voir SetFiltre
			mLastInjectMs = (float32)(NkChrono::Now() - tInj0).ToMilliseconds();
			mDevice->WriteTexture(mVoxelTex, mUpload.Data());
			mHasGI = true;
			mLastLights = lights;

			// Les 5 premiers recalculs sont loggés AVEC leur coût : c'est ce chiffre
			// qui dit si l'indirect peut suivre une géométrie qui bouge.
			static int sLogged = 0;
			if (sLogged++ < 5)
				logger.Info("[NkVoxelAOSystem] GI inject : {0} lumieres -> {1} voxels emetteurs (scale={2}, "
							"shadowSteps={3}) | bake {4} ms + injection {5} ms\n",
							lights.Size(), lit, mCfg.giScale, mCfg.giShadowSteps, mLastBuildMs, mLastInjectMs);
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

		// =====================================================================
		// (09/11/2026, rendu-realiste) BORNES REGLABLES ET VOXELISATION PAR TRIANGLE
		// =====================================================================
		bool NkVoxelAOSystem::ApplyConfig(const NkVoxelAOConfig &cfg) {
			if (!mDevice)
				return false;
			if (cfg.resX == 0 || cfg.resY == 0 || cfg.resZ == 0)
				return false;
			const bool resChange = cfg.resX != mCfg.resX || cfg.resY != mCfg.resY || cfg.resZ != mCfg.resZ;
			if (resChange || !mVoxelTex.IsValid()) {
				if (mVoxelTex.IsValid()) {
					mDevice->DestroyTexture(mVoxelTex);
					mVoxelTex = {};
				}
				NkTextureDesc td = NkTextureDesc::Tex3D(cfg.resX, cfg.resY, cfg.resZ, NkGPUFormat::NK_RGBA8_UNORM);
				td.debugName = "VoxelAO";
				mVoxelTex = mDevice->CreateTexture(td);
				if (!mVoxelTex.IsValid()) {
					logger.Errorf("[NkVoxelAOSystem] Reconfigure : texture 3D refusee (%ux%ux%u)\n", cfg.resX, cfg.resY,
								  cfg.resZ);
					return false;
				}
				const uint32 total = cfg.resX * cfg.resY * cfg.resZ;
				NkVector<uint8> zeros;
				zeros.Resize(total * 4);
				memory::NkSet(zeros.Data(), 0, total * 4);
				mDevice->WriteTexture(mVoxelTex, zeros.Data());
			}
			mCfg = cfg;
			mOpacity.Clear();
			mAlbedoRGB.Clear();
			mUpload.Clear();
			mTriNormal.Clear();
			mLastLights.Clear();
			mHasGI = false;
			mDirty = true;
			return true;
		}

		bool NkVoxelAOSystem::Reconfigure(const math::NkVec3f &mn, const math::NkVec3f &mx, uint32 resX, uint32 resY,
										  uint32 resZ, uint32 giShadowSteps) {
			if (mx.x - mn.x <= 1e-3f || mx.y - mn.y <= 1e-3f || mx.z - mn.z <= 1e-3f)
				return false;
			NkVoxelAOConfig c = mCfg;
			c.minBounds = mn;
			c.maxBounds = mx;
			c.resX = resX;
			c.resY = resY;
			c.resZ = resZ;
			c.giShadowSteps = giShadowSteps;
			if (!ApplyConfig(c))
				return false;
			mBoundsCustom = true;
			logger.Info("[NkVoxelAOSystem] Reconfigure : [{0},{1},{2}]..[{3},{4},{5}] en {6}x{7}x{8}, ombre {9} pas\n", mn.x,
						mn.y, mn.z, mx.x, mx.y, mx.z, resX, resY, resZ, giShadowSteps);
			return true;
		}

		// La part du ciel vue depuis un voxel : `mCielRayons` directions reparties en
		// cosinus autour de la normale (suite de Hammersley, rayon r = sqrt(u), angle
		// 2 pi v : la densite est proportionnelle au cosinus, la moyenne des visibilites
		// est donc directement la part de l'irradiance). Un rayon voit le ciel s'il sort
		// de la grille en montant sans s'etre accumule plus de 90 % d'opacite ; vers le
		// bas, il trouverait le sol hors de la grille : compte comme sombre.
		float32 NkVoxelAOSystem::CielVisible(const math::NkVec3f &c, const math::NkVec3f &n) const {
			const math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
			const float32 vs = size.x / (float32)mCfg.resX;
			const float32 pas = vs * 1.5f;
			// base orthonormee autour de n
			const math::NkVec3f ax = (n.y < 0.9f && n.y > -0.9f) ? math::NkVec3f{0.f, 1.f, 0.f} : math::NkVec3f{1.f, 0.f, 0.f};
			math::NkVec3f t{ax.y * n.z - ax.z * n.y, ax.z * n.x - ax.x * n.z, ax.x * n.y - ax.y * n.x};
			const float32 tl = math::NkSqrt(t.x * t.x + t.y * t.y + t.z * t.z);
			t = math::NkVec3f{t.x / tl, t.y / tl, t.z / tl};
			const math::NkVec3f b{n.y * t.z - n.z * t.y, n.z * t.x - n.x * t.z, n.x * t.y - n.y * t.x};
			const uint32 K = mCielRayons;
			uint32 vus = 0;
			const uint32 pasMax = (uint32)(math::NkSqrt(size.x * size.x + size.y * size.y + size.z * size.z) / pas) + 2u;
			for (uint32 k = 0; k < K; ++k) {
				// Hammersley : u = (k + 0,5) / K, v = inverse binaire de k
				uint32 bits = k;
				bits = (bits << 16u) | (bits >> 16u);
				bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
				bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
				bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
				bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
				const float32 u = ((float32)k + 0.5f) / (float32)K;
				const float32 v = (float32)bits * 2.3283064365386963e-10f;
				const float32 r = math::NkSqrt(u), phi = 6.2831853f * v;
				const float32 lx = r * math::NkCos(phi), ly = r * math::NkSin(phi);
				const float32 lz = math::NkSqrt(1.f - u > 0.f ? 1.f - u : 0.f);
				const math::NkVec3f d{t.x * lx + b.x * ly + n.x * lz, t.y * lx + b.y * ly + n.y * lz,
									  t.z * lx + b.z * ly + n.z * lz};
				float32 bloque = 0.f;
				bool sorti = false;
				for (uint32 i = 2; i < pasMax; ++i) {
					const float32 s = pas * (float32)i;
					const math::NkVec3f p{c.x + d.x * s, c.y + d.y * s, c.z + d.z * s};
					if (p.x < mCfg.minBounds.x || p.y < mCfg.minBounds.y || p.z < mCfg.minBounds.z ||
						p.x >= mCfg.maxBounds.x || p.y >= mCfg.maxBounds.y || p.z >= mCfg.maxBounds.z) {
						sorti = true;
						break;
					}
					const float32 o = OpacityAtWorld(p);
					bloque = bloque + o * (1.f - bloque);
					if (bloque > 0.9f)
						break;
				}
				if (sorti && d.y > 0.f && bloque <= 0.9f)
					++vus;
			}
			return (float32)vus / (float32)K;
		}

		// Flou en boite separable (trois passes X, Y, Z ; somme glissante), rayon mFiltre,
		// bords en repli sur le dernier voxel. Sur rgba8, arrondi au plus proche.
		void NkVoxelAOSystem::Filtrer(NkVector<uint8> &rgba) const {
			const uint32 rx = mCfg.resX, ry = mCfg.resY, rz = mCfg.resZ;
			const int32 r = (int32)mFiltre;
			if (r <= 0 || rgba.Size() != (usize)rx * ry * rz * 4)
				return;
			const uint32 nMax = rx > ry ? (rx > rz ? rx : rz) : (ry > rz ? ry : rz);
			NkVector<uint32> ligne;
			ligne.Resize((usize)nMax * 4);
			const uint32 diam = (uint32)(2 * r + 1);
			// une passe le long d'un axe : n voxels, pas `pas` (en voxels) entre deux voisins
			auto passe = [&](uint32 base, uint32 n, uint32 pas) {
				for (uint32 i = 0; i < n; ++i)
					for (uint32 c = 0; c < 4; ++c)
						ligne[i * 4 + c] = rgba[(base + i * pas) * 4 + c];
				// L'OPACITE (a) se DILATE (maximum sur la fenetre) : une moyenne diviserait
				// un mur d'un voxel par trois et le rendrait translucide -- la lumiere
				// passerait. La radiance (rgb), elle, se moyenne.
				for (uint32 i = 0; i < n; ++i) {
					uint32 m = 0;
					for (int32 k = -r; k <= r; ++k) {
						const int32 j = (int32)i + k;
						if (j < 0 || j >= (int32)n)
							continue;
						const uint32 v = ligne[(uint32)j * 4 + 3];
						if (v > m)
							m = v;
					}
					rgba[(base + i * pas) * 4 + 3] = (uint8)m;
				}
				for (uint32 c = 0; c < 3; ++c) {
					uint32 somme = 0;
					for (int32 k = -r; k <= r; ++k) {
						const int32 j = k < 0 ? 0 : (k >= (int32)n ? (int32)n - 1 : k);
						somme += ligne[(uint32)j * 4 + c];
					}
					for (uint32 i = 0; i < n; ++i) {
						rgba[(base + i * pas) * 4 + c] = (uint8)((somme + diam / 2) / diam);
						const int32 sortant = (int32)i - r, entrant = (int32)i + r + 1;
						const int32 js = sortant < 0 ? 0 : (sortant >= (int32)n ? (int32)n - 1 : sortant);
						const int32 je = entrant < 0 ? 0 : (entrant >= (int32)n ? (int32)n - 1 : entrant);
						somme = somme - ligne[(uint32)js * 4 + c] + ligne[(uint32)je * 4 + c];
					}
				}
			};
			for (uint32 z = 0; z < rz; ++z)
				for (uint32 y = 0; y < ry; ++y)
					passe((z * ry + y) * rx, rx, 1u);
			for (uint32 z = 0; z < rz; ++z)
				for (uint32 x = 0; x < rx; ++x)
					passe(z * ry * rx + x, ry, rx);
			for (uint32 y = 0; y < ry; ++y)
				for (uint32 x = 0; x < rx; ++x)
					passe(y * rx + x, rz, rx * ry);
		}

		bool NkVoxelAOSystem::ResetBounds() {
			if (!mBoundsCustom)
				return true;
			if (!ApplyConfig(mCfgInit))
				return false;
			mBoundsCustom = false;
			return true;
		}

		void NkVoxelAOSystem::RegisterTriangle(const math::NkVec3f &a, const math::NkVec3f &b, const math::NkVec3f &c,
											   const math::NkVec3f &albedo, float32 opacity) {
			NkVoxelTri t;
			t.a = a;
			t.b = b;
			t.c = c;
			t.albedo = albedo;
			t.opacity = opacity;
			mTris.PushBack(t);
			mDirty = true;
		}

		namespace {
			// RECOUVREMENT TRIANGLE-BOITE (Akenine-Moller, « Fast 3D Triangle-Box
			// Overlap Testing », JGT 2001) : 13 axes separateurs -- les 3 axes de la
			// boite, la normale du triangle, et les 9 produits aretes x axes. La boite
			// est centree a l'origine (sommets deja translates), demi-tailles h.
			inline float32 VxMin3(float32 a, float32 b, float32 c) {
				return a < b ? (a < c ? a : c) : (b < c ? b : c);
			}
			inline float32 VxMax3(float32 a, float32 b, float32 c) {
				return a > b ? (a > c ? a : c) : (b > c ? b : c);
			}
			inline float32 VxAbs(float32 x) {
				return x < 0.f ? -x : x;
			}
			bool VxAxeSepare(const math::NkVec3f &ax, const math::NkVec3f &v0, const math::NkVec3f &v1,
							 const math::NkVec3f &v2, const math::NkVec3f &h) {
				const float32 p0 = ax.x * v0.x + ax.y * v0.y + ax.z * v0.z;
				const float32 p1 = ax.x * v1.x + ax.y * v1.y + ax.z * v1.z;
				const float32 p2 = ax.x * v2.x + ax.y * v2.y + ax.z * v2.z;
				const float32 r = h.x * VxAbs(ax.x) + h.y * VxAbs(ax.y) + h.z * VxAbs(ax.z);
				return VxMin3(p0, p1, p2) > r || VxMax3(p0, p1, p2) < -r;
			}
			bool VxTriBoite(const math::NkVec3f &v0, const math::NkVec3f &v1, const math::NkVec3f &v2,
							const math::NkVec3f &h, const math::NkVec3f &n) {
				// axes de la boite
				if (VxMin3(v0.x, v1.x, v2.x) > h.x || VxMax3(v0.x, v1.x, v2.x) < -h.x)
					return false;
				if (VxMin3(v0.y, v1.y, v2.y) > h.y || VxMax3(v0.y, v1.y, v2.y) < -h.y)
					return false;
				if (VxMin3(v0.z, v1.z, v2.z) > h.z || VxMax3(v0.z, v1.z, v2.z) < -h.z)
					return false;
				// plan du triangle
				const float32 d = n.x * v0.x + n.y * v0.y + n.z * v0.z;
				const float32 r = h.x * VxAbs(n.x) + h.y * VxAbs(n.y) + h.z * VxAbs(n.z);
				if (VxAbs(d) > r)
					return false;
				// 9 produits aretes x axes
				const math::NkVec3f e[3] = {{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z},
											{v2.x - v1.x, v2.y - v1.y, v2.z - v1.z},
											{v0.x - v2.x, v0.y - v2.y, v0.z - v2.z}};
				for (int32 i = 0; i < 3; ++i) {
					const math::NkVec3f &E = e[i];
					const math::NkVec3f axes[3] = {{0.f, -E.z, E.y}, {E.z, 0.f, -E.x}, {-E.y, E.x, 0.f}};
					for (int32 k = 0; k < 3; ++k)
						if (VxAxeSepare(axes[k], v0, v1, v2, h))
							return false;
				}
				return true;
			}
		} // namespace

		uint32 NkVoxelAOSystem::VoxelizeTriangles() {
			const uint32 total = mCfg.resX * mCfg.resY * mCfg.resZ;
			if (mOpacity.Size() != total)
				return 0;
			mTriNormal.Resize(total * 3);
			memory::NkSet(mTriNormal.Data(), 0, sizeof(float32) * total * 3);
			const math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
			const math::NkVec3f vs{size.x / (float32)mCfg.resX, size.y / (float32)mCfg.resY, size.z / (float32)mCfg.resZ};
			const math::NkVec3f h{vs.x * 0.5f, vs.y * 0.5f, vs.z * 0.5f};
			uint32 marques = 0;
			for (uint32 ti = 0; ti < mTris.Size(); ++ti) {
				const NkVoxelTri &T = mTris[ti];
				math::NkVec3f mn{VxMin3(T.a.x, T.b.x, T.c.x), VxMin3(T.a.y, T.b.y, T.c.y), VxMin3(T.a.z, T.b.z, T.c.z)};
				math::NkVec3f mx{VxMax3(T.a.x, T.b.x, T.c.x), VxMax3(T.a.y, T.b.y, T.c.y), VxMax3(T.a.z, T.b.z, T.c.z)};
				uint32 x0, y0, z0, x1, y1, z1;
				if (!WorldToVoxelRange(mn, mx, x0, y0, z0, x1, y1, z1))
					continue;
				const math::NkVec3f e0{T.b.x - T.a.x, T.b.y - T.a.y, T.b.z - T.a.z};
				const math::NkVec3f e1{T.c.x - T.a.x, T.c.y - T.a.y, T.c.z - T.a.z};
				math::NkVec3f n{e0.y * e1.z - e0.z * e1.y, e0.z * e1.x - e0.x * e1.z, e0.x * e1.y - e0.y * e1.x};
				const float32 nl = math::NkSqrt(n.x * n.x + n.y * n.y + n.z * n.z);
				if (nl < 1e-12f)
					continue; // triangle degenere : aucune surface
				const math::NkVec3f nu{n.x / nl, n.y / nl, n.z / nl};
				const uint8 op = (uint8)NkClamp(T.opacity * 255.f, 0.f, 255.f);
				const uint8 ar = (uint8)NkClamp(T.albedo.x * 255.f, 0.f, 255.f);
				const uint8 ag = (uint8)NkClamp(T.albedo.y * 255.f, 0.f, 255.f);
				const uint8 ab = (uint8)NkClamp(T.albedo.z * 255.f, 0.f, 255.f);
				for (uint32 z = z0; z <= z1; ++z)
					for (uint32 y = y0; y <= y1; ++y)
						for (uint32 x = x0; x <= x1; ++x) {
							const math::NkVec3f c{mCfg.minBounds.x + ((float32)x + 0.5f) * vs.x,
												  mCfg.minBounds.y + ((float32)y + 0.5f) * vs.y,
												  mCfg.minBounds.z + ((float32)z + 0.5f) * vs.z};
							const math::NkVec3f v0{T.a.x - c.x, T.a.y - c.y, T.a.z - c.z};
							const math::NkVec3f v1{T.b.x - c.x, T.b.y - c.y, T.b.z - c.z};
							const math::NkVec3f v2{T.c.x - c.x, T.c.y - c.y, T.c.z - c.z};
							if (!VxTriBoite(v0, v1, v2, h, n))
								continue;
							const uint32 vi = (z * mCfg.resY + y) * mCfg.resX + x;
							if (op > mOpacity[vi]) {
								if (mOpacity[vi] == 0)
									++marques;
								mOpacity[vi] = op;
								mAlbedoRGB[vi * 3 + 0] = ar;
								mAlbedoRGB[vi * 3 + 1] = ag;
								mAlbedoRGB[vi * 3 + 2] = ab;
							}
							// la normale ne compte que pour les surfaces opaques
							if (op >= 128) {
								mTriNormal[vi * 3 + 0] += nu.x;
								mTriNormal[vi * 3 + 1] += nu.y;
								mTriNormal[vi * 3 + 2] += nu.z;
							}
						}
			}
			logger.Info("[NkVoxelAOSystem] {0} triangles voxelises -> {1} voxels neufs\n", (uint32)mTris.Size(), marques);
			return marques;
		}

	} // namespace renderer
} // namespace nkentseu
