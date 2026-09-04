// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkVFXSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include <cstdio>
#include <cmath> // sqrt du repli (2026-09-04) // repli de melange dit une fois (2026-09-04)
#include "NkVFXSystem.h"
#include "NKRenderer/Shader/NkShaderLibrary.h" // shader des particules (2026-09-04)
#include "NKLogger/NkLog.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKMath/NKMath.h"
#include "NKMemory/NkAllocator.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "NKTime/NkChrono.h" // les chronos de la mesure (2026-09-04) -- en DERNIER : son namespace `time` masquerait time(nullptr) de NkRandom.h

namespace nkentseu {
	namespace renderer {

		// Pseudo-random fast [0,1]
		static inline float32 NkRandF() {
			static uint32 s = 12345;
			s = s * 1664525u + 1013904223u;
			return math::NkRand.NextFloat32();
		}

		static inline float32 NkRandRange(float32 lo, float32 hi) {
			return math::NkRand.NextFloat32(lo, hi);
		}

		static inline NkVec3f NkRandDir() {
			float32 th = NkRandF() * 2 * math::NK_PI_F, phi = NkRandF() * math::NK_PI_F;
			return {math::NkSin(phi) * math::NkCos(th), math::NkCos(phi), math::NkSin(phi) * math::NkSin(th)};
		}

		// ─────────────────────────────────────────────────────────────────────────
		NkVFXSystem::~NkVFXSystem() {
			Shutdown();
		}

		bool NkVFXSystem::Init(NkIDevice *device, NkTextureLibrary *texLib, NkMeshSystem *mesh,
							   NkShaderLibrary *shaderLib) {
			mDevice = device;
			mTexLib = texLib;
			mMesh = mesh;
			mShaderLib = shaderLib;

			// ── LE SHADER DES PARTICULES (2026-09-04) ────────────────────────
			// Avant : les trois pipelines VFX n'avaient NI shader, NI vertexLayout.
			// CreateGraphicsPipeline rend {} quand d.shader est introuvable, donc
			// BindGraphicsPipeline ne liait rien et Draw partait sans programme.
			// Le defaut etait invisible parce que personne n'appelait Update() :
			// aliveCount == 0 court-circuitait le dessin AVANT qu'il ne puisse
			// echouer. Un defaut en cachait un autre.
			::nkentseu::NkShaderHandle particleShader;
			if (mShaderLib) {
				auto prog = mShaderLib->LoadOrCompileVF("Particles", "", "");
				if (prog.IsValid())
					particleShader = mShaderLib->GetRHIHandle(prog);
			}

			// Trois pipelines de particules, un par famille de melange que NkBlendDesc
			// sait fabriquer -- NkEmitterDesc::blend choisit a l'appel (2026-09-04).
			// Borne 2 : le layout de la texture de particule (binding 1 ; le binding 0 est
			// uCam, lie par nom sur le chemin aplati GL -- le jeu global Vulkan est la borne 3).
			{
				NkDescriptorSetLayoutDesc tl;
				tl.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
				tl.debugName = "ParticlesTexture";
				mTexLayout = mDevice->CreateDescriptorSetLayout(tl);
			}
			// Le repli : le disque doux qui etait code en dur dans le fragment, devenu une
			// texture blanche 32x32 a alpha radial. Un emetteur sans texture le recoit, et
			// on le dit (une fois par emetteur) -- jamais un blanc silencieux.
			if (mTexLib && !mFallbackTex.IsValid()) {
				const uint32 S = 32;
				NkVector<uint8> px;
				px.Resize(S * S * 4);
				for (uint32 y = 0; y < S; ++y)
					for (uint32 x = 0; x < S; ++x) {
						const float32 dx = ((float32)x + 0.5f) / (float32)S * 2.f - 1.f;
						const float32 dy = ((float32)y + 0.5f) / (float32)S * 2.f - 1.f;
						const float32 r = (float32)std::sqrt((double)(dx * dx + dy * dy));
						float32 a = (r - 0.55f) / 0.45f;
						a = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
						a = 1.f - a; // = 1 - smoothstep lineaire, comme le fragment d'avant
						uint8 *p = &px[(y * S + x) * 4];
						p[0] = p[1] = p[2] = 255;
						p[3] = (uint8)(a * 255.f + 0.5f);
					}
				NkTextureCreateDesc td;
				td.pixels = px.Data();
				td.width = S;
				td.height = S;
				td.debugName = "ParticlesFallbackDisc";
				mFallbackTex = mTexLib->Create(td);
			}
			const NkBlendDesc familles[3] = {NkBlendDesc::Additive(), NkBlendDesc::Alpha(), NkBlendDesc::Opaque()};
			const char *noms[3] = {"ParticlesBillboard.Additive", "ParticlesBillboard.Alpha", "ParticlesBillboard.Opaque"};
			for (uint32 f = 0; f < 3; ++f) {
					NkGraphicsPipelineDesc pd;
					pd.rasterizer = NkRasterizerDesc::Default();
					pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_NONE;
					pd.depthStencil = NkDepthStencilDesc::Default();
					pd.depthStencil.depthWriteEnable = false;
					pd.blend = familles[f];
					pd.debugName = noms[f];
					pd.shader = particleShader;
					if (mTexLayout.IsValid())
						pd.descriptorSetLayouts.PushBack(mTexLayout); // borne 2 : la texture
					// INSTANCIATION (2026-09-04, soir) : binding 0 = le coin, PAR SOMMET, six
					// vec2 statiques partages par tous les emetteurs ; binding 1 = la
					// particule, PAR INSTANCE (NkParticleInstance, 24 o). Le CPU n'ecrit plus
					// six sommets de 32 o par particule ; le VS expanse le coin depuis
					// aCorner + aSize. Draw(6, vivantes).
					pd.vertexLayout.AddBinding(0, sizeof(NkVec2f), false)
						.AddBinding(1, sizeof(NkParticleInstance), true)
						.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0, "TEXCOORD", 0)
						.AddAttribute(1, 1, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
						.AddAttribute(2, 1, NkGPUFormat::NK_RGBA8_UNORM, 16, "COLOR", 0)
						.AddAttribute(3, 1, NkGPUFormat::NK_R32_FLOAT, 12, "TEXCOORD", 1)
						.AddAttribute(4, 1, NkGPUFormat::NK_R32_FLOAT, 20, "TEXCOORD", 2);
					mPipeParticle[f] = mDevice->CreateGraphicsPipeline(pd);
					if (!mPipeParticle[f].IsValid())
						logger.Errorf("[NkVFXSystem] pipeline particules INVALIDE (shader_valid=%d) -- "
									  "rien ne se dessinera\n",
									  particleShader.IsValid() ? 1 : 0);
			}
			// Les six coins du quad (deux triangles, TRIANGLE_LIST), STATIQUES : un
			// seul tampon de 48 octets pour tous les emetteurs, cree une fois.
			{
				static const NkVec2f kCorners[6] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
				mQuadVB = mDevice->CreateBuffer(NkBufferDesc::Vertex(sizeof(kCorners), kCorners));
				if (!mQuadVB.IsValid())
					logger.Errorf("[NkVFXSystem] tampon des coins du quad INVALIDE -- aucune particule ne se dessinera\n");
			}
			{
				NkGraphicsPipelineDesc pd;
				pd.rasterizer = NkRasterizerDesc::Default();
				pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_NONE;
				pd.depthStencil = NkDepthStencilDesc::Default();
				pd.depthStencil.depthWriteEnable = false;
				pd.blend = NkBlendDesc::Alpha();
				pd.debugName = "TrailMesh";
				mPipeTrail = mDevice->CreateGraphicsPipeline(pd);
			}
			{
				NkGraphicsPipelineDesc pd;
				pd.rasterizer = NkRasterizerDesc::Default();
				pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_NONE;
				pd.depthStencil = NkDepthStencilDesc::Default();
				pd.depthStencil.depthWriteEnable = false;
				pd.blend = NkBlendDesc::Alpha();
				pd.debugName = "Decal";
				mPipeDecal = mDevice->CreateGraphicsPipeline(pd);
			}

			return true;
		}

		void NkVFXSystem::Shutdown() {
			for (auto *e : mEmitters) {
				if (e->store) {
					e->store->Shutdown(mDevice);
					memory::NkGetDefaultAllocator().Delete(e->store);
				}
				memory::NkGetDefaultAllocator().Delete(e);
			}
			for (auto *t : mTrails) {
				if (t->vbo.IsValid())
					mDevice->DestroyBuffer(t->vbo);
				memory::NkGetDefaultAllocator().Delete(t);
			}
			for (auto *d : mDecals)
				memory::NkGetDefaultAllocator().Delete(d);
			if (mQuadVB.IsValid()) {
				mDevice->DestroyBuffer(mQuadVB);
				mQuadVB = {};
			}
			mEmitters.Clear();
			mTrails.Clear();
			mDecals.Clear();
		}

		// ── Émetteurs ─────────────────────────────────────────────────────────────
		// La cible demandee -> la cible obtenue, DITE quand elle differe (une fois par
		// emetteur). Le stockage GPU n'est pas livre : tout retombe sur CPU aujourd'hui,
		// et la ligne au journal dit pourquoi (pas de compute / pas encore livre).
		static NkSimTarget NkResolveSimTarget(NkIDevice *dev, NkSimTarget demande, uint64 id) {
			const bool compute = dev && dev->GetCaps().computeShaders;
			if (demande == NkSimTarget::CPU)
				return NkSimTarget::CPU;
			if (!compute) {
				if (demande == NkSimTarget::AUTO)
					std::fprintf(stderr, "[NkVFX] emetteur %llu : simTarget=AUTO -> CPU (pas de compute sur ce device)\n",
								 (unsigned long long)id);
				else
					std::fprintf(stderr, "[NkVFX] emetteur %llu : simTarget=GPU demande, pas de compute sur ce device -> CPU\n",
								 (unsigned long long)id);
				return NkSimTarget::CPU;
			}
			// Compute present : le stockage GPU viendrait ici (plan (B)). Pas livre -> dit.
			std::fprintf(stderr, "[NkVFX] emetteur %llu : simTarget=%s, compute present mais stockage GPU pas encore livre -> CPU\n",
						 (unsigned long long)id, demande == NkSimTarget::AUTO ? "AUTO" : "GPU");
			return NkSimTarget::CPU;
		}

		NkEmitterId NkVFXSystem::CreateEmitter(const NkEmitterDesc &desc) {
			Emitter *e = memory::NkGetDefaultAllocator().New<Emitter>();
			e->id = {mNextId++};
			e->desc = desc;
			e->enabled = true;
			e->spawnAccum = 0.f;
			e->resolved = NkResolveSimTarget(mDevice, desc.simTarget, e->id.id);
			// Le stockage (CPU SoA ; le GPU viendra derriere la meme interface).
			{
				auto *cpu = memory::NkGetDefaultAllocator().New<NkParticleStoreCPU>();
				cpu->solver = desc.solver;
				if (!cpu->Init(mDevice, desc))
					logger.Errorf("[NkVFXSystem] emetteur %llu : stockage CPU sans tampon d'instances -- rien ne se dessinera\n",
								  (unsigned long long)e->id.id);
				e->store = cpu;
			}
			e->births.Reserve(64);

			// Borne 2 : la texture DECLAREE est celle qui rend ; sans texture, le repli, DIT.
			if (mTexLayout.IsValid() && mTexLib) {
				e->texSet = mDevice->AllocateDescriptorSet(mTexLayout);
				NkTexHandle tex = desc.texture;
				if (!tex.IsValid()) {
					tex = mFallbackTex;
					std::fprintf(stderr, "[NkVFX] emetteur %llu sans texture : repli disque doux blanc 32x32\n",
								 (unsigned long long)e->id.id);
				}
				if (e->texSet.IsValid() && tex.IsValid())
					mDevice->BindTextureSampler(e->texSet, 1, mTexLib->GetRHIHandle(tex), mTexLib->GetRHISampler(tex));
			}
			mEmitters.PushBack(e);
			return e->id;
		}

		void NkVFXSystem::DestroyEmitter(NkEmitterId &id) {
			for (uint32 i = 0; i < (uint32)mEmitters.Size(); i++) {
				if (mEmitters[i]->id.id == id.id) {
					if (mEmitters[i]->store) {
						mEmitters[i]->store->Shutdown(mDevice);
						memory::NkGetDefaultAllocator().Delete(mEmitters[i]->store);
					}
					if (mEmitters[i]->texSet.IsValid())
						mDevice->FreeDescriptorSet(mEmitters[i]->texSet);
					memory::NkGetDefaultAllocator().Delete(mEmitters[i]);
					mEmitters.RemoveAt(i);
					break;
				}
			}
			id.id = 0;
		}

		void NkVFXSystem::SetEmitterPos(NkEmitterId id, NkVec3f pos) {
			for (auto *e : mEmitters)
				if (e->id.id == id.id) {
					e->desc.position = pos;
					return;
				}
		}

		void NkVFXSystem::SetEmitterEnabled(NkEmitterId id, bool on) {
			for (auto *e : mEmitters)
				if (e->id.id == id.id) {
					e->enabled = on;
					return;
				}
		}

		NkEmitterDesc *NkVFXSystem::GetEmitterDesc(NkEmitterId id) {
			for (auto *e : mEmitters)
				if (e->id.id == id.id)
					return &e->desc;
			return nullptr;
		}

		void NkVFXSystem::Burst(NkEmitterId id, uint32 count) {
			for (auto *e : mEmitters) {
				if (e->id.id == id.id) {
					uint32 n = (count > 0) ? count : (uint32)e->desc.burstCount;
					for (uint32 i = 0; i < n; i++)
						SpawnParticle(e);
					if (e->store && !e->births.Empty()) {
						e->store->Spawn(e->births.Data(), (uint32)e->births.Size());
						e->births.Clear();
					}
					return;
				}
			}
		}

		void NkVFXSystem::SpawnBirths(NkEmitterId id, const NkParticleBirth *births, uint32 n) {
			if (!births || n == 0)
				return;
			for (auto *e : mEmitters)
				if (e->id.id == id.id && e->store) {
					e->store->Spawn(births, n);
					return;
				}
		}

		void NkVFXSystem::SpawnParticle(Emitter *e) {
			// Le CPU decide QUI nait (forme, alea, vitesse) pour toutes les cibles ; le
			// stockage recoit une NkParticleBirth. Memes tirages, meme ordre qu'avant.
			NkParticleBirth b;
			b.life = NkRandRange(e->desc.lifeMin, e->desc.lifeMax);
			b.rotation = NkRandF() * 2 * math::NK_PI_F;
			b.rotSpeed = NkRandRange(-2.f, 2.f);
			switch (e->desc.shape) {
				case NkEmitterShape::SPHERE:
					b.pos = {e->desc.position.x + NkRandDir().x * e->desc.radius,
							 e->desc.position.y + NkRandDir().y * e->desc.radius,
							 e->desc.position.z + NkRandDir().z * e->desc.radius};
					break;
				case NkEmitterShape::BOX: {
					NkVec3f bx = e->desc.boxSize;
					b.pos = {e->desc.position.x + NkRandRange(-bx.x, bx.x) * 0.5f,
							 e->desc.position.y + NkRandRange(-bx.y, bx.y) * 0.5f,
							 e->desc.position.z + NkRandRange(-bx.z, bx.z) * 0.5f};
					break;
				}
				default:
					b.pos = e->desc.position;
					break;
			}
			NkVec3f d = {
				e->desc.velocityDir.x * (1.f - e->desc.velocityRand) + NkRandDir().x * e->desc.velocityRand,
				e->desc.velocityDir.y * (1.f - e->desc.velocityRand) + NkRandDir().y * e->desc.velocityRand,
				e->desc.velocityDir.z * (1.f - e->desc.velocityRand) + NkRandDir().z * e->desc.velocityRand,
			};
			float32 spd = NkRandRange(e->desc.speedMin, e->desc.speedMax);
			float32 len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
			if (len > 1e-5f) {
				d.x /= len;
				d.y /= len;
				d.z /= len;
			}
			b.vel = {d.x * spd, d.y * spd, d.z * spd};
			e->births.PushBack(b);
		}

		// ── Update ────────────────────────────────────────────────────────────────
		void NkVFXSystem::Update(float32 dt, const NkCamera3DData &cam) {
			mTotalParticles = 0;
			mProfile = NkVFXProfile{}; // la mesure repart a chaque image
			mProfile = NkVFXProfile{}; // la mesure repart a chaque image
			mProfile = NkVFXProfile{}; // la mesure repart a chaque image
			for (auto *e : mEmitters) {
				UpdateEmitter(e, dt, cam);
				mTotalParticles += e->store ? e->store->AliveCount() : 0u;
			}
			for (auto *t : mTrails)
				UpdateTrail(t, dt);
			// Age des decals
			for (uint32 i = 0; i < (uint32)mDecals.Size();) {
				mDecals[i]->age += dt;
				if (mDecals[i]->desc.lifetime > 0 && mDecals[i]->age > mDecals[i]->desc.lifetime) {
					memory::NkGetDefaultAllocator().Delete(mDecals[i]);
					mDecals.RemoveAt(i);
				} else
					i++;
			}
		}

		void NkVFXSystem::UpdateEmitter(Emitter *e, float32 dt, const NkCamera3DData &cam) {
			(void)cam;
			if (!e->store)
				return;
			// Naissances de l'image (CPU, toujours), poussees d'un coup au stockage.
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			if (e->enabled && e->desc.ratePerSec > 0.f) {
				e->spawnAccum += e->desc.ratePerSec * dt;
				while (e->spawnAccum >= 1.f) {
					SpawnParticle(e);
					++mProfile.spawned;
					e->spawnAccum -= 1.f;
				}
			}
			if (!e->births.Empty()) {
				e->store->Spawn(e->births.Data(), (uint32)e->births.Size());
				e->births.Clear();
			}
			const int64 t1 = ::nkentseu::NkChrono::Now().nanoseconds;
			mProfile.spawnMs += (float32)((t1 - t0) / 1.0e6);
			// Le pas : integration + instances + envoi (CPU) -- ou Dispatch (GPU, plan).
			NkParticleStepStats st;
			e->store->Step(nullptr, e->desc, dt, st);
			mProfile.simMs += st.simMs;
			mProfile.buildMs += st.buildMs;
			mProfile.uploadMs += st.uploadMs;
			mProfile.uploadBytes += st.uploadBytes;
			mProfile.alive += st.alive;
		}

		// ── Trails ────────────────────────────────────────────────────────────────
		NkTrailId NkVFXSystem::CreateTrail(const NkTrailDesc &desc) {
			Trail *t = memory::NkGetDefaultAllocator().New<Trail>();
			t->id = {mNextId++};
			t->desc = desc;
			t->vbo = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic(desc.maxPoints * sizeof(NkVertex3D) * 2));
			mTrails.PushBack(t);
			return t->id;
		}

		void NkVFXSystem::DestroyTrail(NkTrailId &id) {
			for (uint32 i = 0; i < (uint32)mTrails.Size(); i++) {
				if (mTrails[i]->id.id == id.id) {
					if (mTrails[i]->vbo.IsValid())
						mDevice->DestroyBuffer(mTrails[i]->vbo);
					memory::NkGetDefaultAllocator().Delete(mTrails[i]);
					mTrails.RemoveAt(i);
					break;
				}
			}
			id.id = 0;
		}

		void NkVFXSystem::AddTrailPoint(NkTrailId id, NkVec3f pos) {
			for (auto *t : mTrails) {
				if (t->id.id != id.id)
					continue;
				// Ne pas ajouter si trop proche du dernier point
				if (!t->points.Empty()) {
					auto &last = t->points[t->points.Size() - 1];
					float32 dx = pos.x - last.pos.x, dy = pos.y - last.pos.y, dz = pos.z - last.pos.z;
					if (sqrtf(dx * dx + dy * dy + dz * dz) < t->desc.minDistance)
						return;
				}
				TrailPoint tp;
				tp.pos = pos;
				tp.time = 0.f;
				if ((uint32)t->points.Size() >= t->desc.maxPoints)
					t->points.RemoveAt(0);
				t->points.PushBack(tp);
				return;
			}
		}

		void NkVFXSystem::ClearTrail(NkTrailId id) {
			for (auto *t : mTrails)
				if (t->id.id == id.id) {
					t->points.Clear();
					return;
				}
		}

		void NkVFXSystem::UpdateTrail(Trail *t, float32 dt) {
			// Vieillir les points
			for (auto &p : t->points)
				p.time += dt;
			// Supprimer points trop vieux
			while (!t->points.Empty() && t->points[0].time > t->desc.lifetime)
				t->points.RemoveAt(0);
			if (t->points.Size() < 2)
				return;
			// Rebuild ribbon mesh
			NkVector<NkVertex3D> verts;
			verts.Reserve(t->points.Size() * 2);
			uint32 n = (uint32)t->points.Size();
			for (uint32 i = 0; i < n; i++) {
				float32 u = (float32)i / (n - 1);
				float32 age = t->points[i].time / t->desc.lifetime;
				NkVec4f col = {
					t->desc.colorStart.x + (t->desc.colorEnd.x - t->desc.colorStart.x) * age,
					t->desc.colorStart.y + (t->desc.colorEnd.y - t->desc.colorStart.y) * age,
					t->desc.colorStart.z + (t->desc.colorEnd.z - t->desc.colorStart.z) * age,
					t->desc.colorStart.w + (t->desc.colorEnd.w - t->desc.colorStart.w) * age,
				};
				uint32 c = ((uint32)(col.w * 255) << 24) | ((uint32)(col.z * 255) << 16) |
						   ((uint32)(col.y * 255) << 8) | (uint32)(col.x * 255);
				// Direction perpendiculaire au trail (simplifiée : up world)
				NkVec3f perp = {t->desc.width * 0.5f, 0, 0};
				NkVertex3D vL, vR;
				vL.pos = {t->points[i].pos.x - perp.x, t->points[i].pos.y, t->points[i].pos.z};
				vR.pos = {t->points[i].pos.x + perp.x, t->points[i].pos.y, t->points[i].pos.z};
				vL.normal = vR.normal = {0, 0, 1};
				vL.tangent = vR.tangent = {1, 0, 0};
				vL.uv = {u, 0};
				vR.uv = {u, 1};
				vL.color = vR.color = c;
				verts.PushBack(vL);
				verts.PushBack(vR);
			}
			mDevice->WriteBuffer(t->vbo, verts.Data(), (uint32)verts.Size() * sizeof(NkVertex3D));
		}

		// ── Decals ────────────────────────────────────────────────────────────────
		NkDecalId NkVFXSystem::SpawnDecal(const NkDecalDesc &desc) {
			Decal *d = memory::NkGetDefaultAllocator().New<Decal>();
			d->id = {mNextId++};
			d->desc = desc;
			mDecals.PushBack(d);
			return d->id;
		}

		void NkVFXSystem::DestroyDecal(NkDecalId &id) {
			for (uint32 i = 0; i < (uint32)mDecals.Size(); i++) {
				if (mDecals[i]->id.id == id.id) {
					memory::NkGetDefaultAllocator().Delete(mDecals[i]);
					mDecals.RemoveAt(i);
					break;
				}
			}
			id.id = 0;
		}

		// ── Render ────────────────────────────────────────────────────────────────
		void NkVFXSystem::Render(NkICommandBuffer *cmd, const NkCamera3DData &cam) {
			for (auto *e : mEmitters)
				if (e->store && e->store->DrawCount() > 0)
					RenderEmitter(cmd, e, cam);
			for (auto *t : mTrails)
				if (t->points.Size() > 1)
					RenderTrail(cmd, t, cam);
			if (!mDecals.Empty())
				RenderDecals(cmd);
		}

		NkPipelineHandle NkVFXSystem::PipelineFor(NkBlendMode mode) {
			switch (mode) {
			case NkBlendMode::NK_ADDITIVE: return mPipeParticle[0];
			case NkBlendMode::NK_ALPHA: return mPipeParticle[1];
			case NkBlendMode::NK_OPAQUE: return mPipeParticle[2];
			default: break;
			}
			// MULTIPLY / PREMULT / SCREEN : NkBlendDesc n'a pas de fabrique pour eux.
			// Repli Alpha, dit UNE fois par mode -- une degradation qui se tait est
			// exactement le defaut qu'on vient de corriger.
			const uint32 k = (uint32)mode < 8u ? (uint32)mode : 7u;
			if (!mBlendFallbackDit[k]) {
				mBlendFallbackDit[k] = true;
				std::fprintf(stderr, "[NkVFX] NkBlendMode %u non honore (pas de fabrique NkBlendDesc) : rendu en Alpha\n",
							 (unsigned)mode);
			}
			return mPipeParticle[1];
		}

		void NkVFXSystem::RenderEmitter(NkICommandBuffer *cmd, Emitter *e, const NkCamera3DData &cam) {
			(void)cam;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			cmd->BindGraphicsPipeline(PipelineFor(e->desc.blend)); // le melange DECLARE est celui qui rend
			if (e->texSet.IsValid())
				cmd->BindDescriptorSet(e->texSet, 0); // la texture DECLAREE (ou le repli dit)
			cmd->BindVertexBuffer(0, mQuadVB, 0);                  // les six coins, statiques, partages
			cmd->BindVertexBuffer(1, e->store->InstanceBuffer(), 0); // un enregistrement de 24 o par instance, du stockage
			cmd->Draw(6, e->store->DrawCount(), 0, 0);             // six sommets x N instances : le quad s'expanse sur le GPU
			mProfile.drawMs += (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
		}

		void NkVFXSystem::RenderTrail(NkICommandBuffer *cmd, Trail *t, const NkCamera3DData &cam) {
			(void)cam;
			if (t->points.Size() < 2)
				return;
			cmd->BindGraphicsPipeline(mPipeTrail);
			cmd->BindVertexBuffer(0, t->vbo, 0);
			uint32 n = (uint32)t->points.Size() * 2;
			cmd->Draw(n, 1, 0, 0);
		}

		void NkVFXSystem::RenderDecals(NkICommandBuffer *cmd) {
			cmd->BindGraphicsPipeline(mPipeDecal);
			for (auto *d : mDecals) {
				struct DecalPC {
						NkMat4f transform;
						float32 opacity;
						float32 normalBlend;
						float32 _p[2];
				} ub;

				ub.transform = d->desc.transform;
				ub.opacity = d->desc.opacity;
				if (d->desc.fadeOut && d->desc.lifetime > 0)
					ub.opacity *= 1.f - (d->age / d->desc.lifetime);
				ub.normalBlend = d->desc.normalBlend;
				cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(ub), &ub);
				cmd->Draw(36, 1, 0, 0);
			}
		}

	} // namespace renderer
} // namespace nkentseu
