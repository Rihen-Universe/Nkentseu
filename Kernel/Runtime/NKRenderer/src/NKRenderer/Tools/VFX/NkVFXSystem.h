#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkVFXSystem.h  — NKRenderer v4.0  (Tools/VFX/)
// Particules CPU/GPU, trails, decals projetés, lens flares.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
	namespace renderer {

		class NkMeshSystem;
		class NkShaderLibrary;

		// =========================================================================
		// Descripteur d'émetteur
		// =========================================================================
		enum class NkEmitterShape : uint8 {
			POINT,
			SPHERE,
			BOX,
			CONE,
			DISK,
			EDGE,
		};
		enum class NkSimMode : uint8 { CPU, GPU };

		struct NkEmitterDesc {
				NkEmitterShape shape = NkEmitterShape::POINT;
				NkVec3f position = {0, 0, 0};
				float32 radius = 1.f;
				NkVec3f boxSize = {1, 1, 1};
				float32 coneAngle = 25.f;
				float32 ratePerSec = 20.f;
				float32 burstCount = 0.f;
				float32 lifeMin = 1.f;
				float32 lifeMax = 2.5f;
				float32 speedMin = 1.f;
				float32 speedMax = 4.f;
				float32 sizeStart = 0.15f;
				float32 sizeEnd = 0.f;
				NkVec4f colorStart = {1, 1, 1, 1};
				NkVec4f colorEnd = {1, 1, 1, 0};
				NkVec3f gravity = {0, -9.8f, 0};
				NkVec3f velocityDir = {0, 1, 0}; // direction initiale
				float32 velocityRand = 1.f;		 // 0=précis, 1=aléatoire
				NkTexHandle texture;
				NkBlendMode blend = NkBlendMode::NK_ADDITIVE;
				NkSimMode simMode = NkSimMode::CPU;
				uint32 maxParticles = 1000;
				bool worldSpace = true;
				bool loop = true;
		};

		struct NkEmitterId {
				uint64 id = 0;

				bool IsValid() const {
					return id != 0;
				}
		};

		// =========================================================================
		// Trail
		// =========================================================================
		struct NkTrailDesc {
				float32 width = 0.1f;
				float32 lifetime = 0.5f;
				uint32 maxPoints = 64;
				NkVec4f colorStart = {1, 1, 1, 1};
				NkVec4f colorEnd = {1, 1, 1, 0};
				NkTexHandle texture;
				NkBlendMode blend = NkBlendMode::NK_ADDITIVE;
				float32 minDistance = 0.05f; // distance min entre deux points
		};

		struct NkTrailId {
				uint64 id = 0;

				bool IsValid() const {
					return id != 0;
				}
		};

		// =========================================================================
		// Decal
		// =========================================================================
		struct NkDecalDesc {
				NkMat4f transform; // position, orientation, taille
				NkTexHandle albedo;
				NkTexHandle normal;
				float32 opacity = 1.f;
				float32 normalBlend = 0.5f;
				float32 lifetime = -1.f; // -1 = permanent
				bool fadeOut = true;
				uint32 layerMask = 0xFFFFFFFF;
		};

		struct NkDecalId {
				uint64 id = 0;

				bool IsValid() const {
					return id != 0;
				}
		};

		// =========================================================================
		// NkVFXSystem
		// =========================================================================
		class NkVFXSystem {
			public:
				NkVFXSystem() = default;
				~NkVFXSystem();

				bool Init(NkIDevice *device, NkTextureLibrary *texLib, NkMeshSystem *mesh,
						  NkShaderLibrary *shaderLib);
				void Shutdown();

				// ── Émetteurs ─────────────────────────────────────────────────────────
				NkEmitterId CreateEmitter(const NkEmitterDesc &desc);
				void DestroyEmitter(NkEmitterId &id);
				void SetEmitterPos(NkEmitterId id, NkVec3f pos);
				void SetEmitterEnabled(NkEmitterId id, bool on);
				void Burst(NkEmitterId id, uint32 count = 0);
				NkEmitterDesc *GetEmitterDesc(NkEmitterId id);

				// ── Trails ────────────────────────────────────────────────────────────
				NkTrailId CreateTrail(const NkTrailDesc &desc);
				void DestroyTrail(NkTrailId &id);
				void AddTrailPoint(NkTrailId id, NkVec3f point);
				void ClearTrail(NkTrailId id);

				// ── Decals ────────────────────────────────────────────────────────────
				NkDecalId SpawnDecal(const NkDecalDesc &desc);
				void DestroyDecal(NkDecalId &id);

				// ── Update & Render ───────────────────────────────────────────────────
				void Update(float32 dt, const NkCamera3DData &cam);
				void Render(NkICommandBuffer *cmd, const NkCamera3DData &cam);

				// ── Stats ──────────────────────────────────────────────────────────────
				// LA MESURE (2026-09-04) : ou passent les millisecondes CPU d une image de
				// particules -- naissance (recherche d un emplacement libre), integration
				// (vie, vitesse, position, couleur), construction des sommets, envoi au GPU,
				// emission des commandes. Quatre chiffres, pas une impression.
				struct NkVFXProfile {
						float32 spawnMs = 0.f, simMs = 0.f, buildMs = 0.f, uploadMs = 0.f, drawMs = 0.f;
						uint32 alive = 0, spawned = 0, uploadBytes = 0;
				};
				const NkVFXProfile &Profile() const {
					return mProfile;
				}
				uint32 GetActiveParticleCount() const {
					return mTotalParticles;
				}

				uint32 GetActiveEmitterCount() const {
					return (uint32)mEmitters.Size();
				}

			private:
				// ── Particule (CPU) ───────────────────────────────────────────────────
				struct Particle {
						NkVec3f pos, vel;
						NkVec4f color;
						float32 size, life, maxLife, rotation, rotSpeed;
						bool alive = false;
				};

				struct Emitter {
						NkEmitterId id;
						NkEmitterDesc desc;
						NkVector<Particle> particles;
						float32 spawnAccum = 0.f;
						bool enabled = true;
						NkBufferHandle vbo; // GPU billboard VBO
						uint32 aliveCount = 0;
						NkDescSetHandle texSet; // la texture de l'emetteur (ou le repli), binding 1 (2026-09-04)
						// Pile des emplacements LIBRES (2026-09-04) : la naissance etait un balayage
						// lineaire de `particles` a chaque particule nee -- mesure : 6 a 793 ms par
						// image a 50 000, tout le reste (integration, sommets, envoi) sous 6 ms.
						NkVector<uint32> freeSlots;
				};

				struct TrailPoint {
						NkVec3f pos;
						float32 time;
				};

				struct Trail {
						NkTrailId id;
						NkTrailDesc desc;
						NkVector<TrailPoint> points;
						NkBufferHandle vbo;
				};

				struct Decal {
						NkDecalId id;
						NkDecalDesc desc;
						float32 age = 0.f;
				};

				NkIDevice *mDevice = nullptr;
				NkShaderLibrary *mShaderLib = nullptr; // shader des particules (2026-09-04)
				NkTextureLibrary *mTexLib = nullptr;
				NkMeshSystem *mMesh = nullptr;

				NkVector<Emitter *> mEmitters;
				NkVector<Trail *> mTrails;
				NkVector<Decal *> mDecals;
				uint64 mNextId = 1;
				uint32 mTotalParticles = 0;

				// Un pipeline par famille de melange (2026-09-04) : NkEmitterDesc::blend
				// etait declare et jamais lu -- tout partait en Additive. Trois familles
				// que NkBlendDesc sait fabriquer : [0] Additive, [1] Alpha, [2] Opaque.
				NkPipelineHandle mPipeParticle[3];
				// Borne 2 (2026-09-04) : NkEmitterDesc::texture etait declaree et jamais lue.
				// Un layout {binding 1 : image+sampler} partage par les trois pipelines, un
				// descripteur par emetteur, et un repli (disque doux blanc 32x32) DIT une fois.
				NkDescSetHandle mTexLayout;
				NkVFXProfile mProfile;
				NkVector<NkVertexParticle> mScratchVerts; // tampon de sommets reutilise (9,6 Mo realloues par image a 50 000, avant)
				NkTexHandle mFallbackTex;
				NkPipelineHandle PipelineFor(NkBlendMode mode);
				bool mBlendFallbackDit[8] = {};
				NkPipelineHandle mPipeTrail;
				NkPipelineHandle mPipeDecal;

				void UpdateEmitter(Emitter *e, float32 dt, const NkCamera3DData &cam);
				void SpawnParticle(Emitter *e);
				void RenderEmitter(NkICommandBuffer *cmd, Emitter *e, const NkCamera3DData &cam);
				void UpdateTrail(Trail *t, float32 dt);
				void RenderTrail(NkICommandBuffer *cmd, Trail *t, const NkCamera3DData &cam);
				void RenderDecals(NkICommandBuffer *cmd);
		};

	} // namespace renderer
} // namespace nkentseu
