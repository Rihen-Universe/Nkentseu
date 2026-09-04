// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkParticleStoreGPU.cpp — stockage GPU des particules ordinaires (2026-09-05), voir le .h.
// Chaîne de compilation du noyau copiée de NkIBLCompute::CompileKernel (le précédent) :
// NkSL @stage(compute) -> GLSL, puis HLSL / SPIR-V / MSL selon le device ; refus dit.
// Mêmes formules que NkParticleStoreCPU::Step (vie, gravité, position, rotation, couleur
// et taille interpolées, couleur RGBA8 empaquetée) : les deux cibles doivent donner la
// même image, c'est le témoin.
// =============================================================================
#include "NkParticleStoreGPU.h"
#include "NkVFXSystem.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include <cstdio>
#include <cstdlib>
#include "NKTime/NkChrono.h" // en dernier (cf. NkVFXSystem.cpp : namespace `time`)

namespace nkentseu {
	namespace renderer {

		// Le noyau : par emplacement (mode 1) ou par naissance (mode 0). Les instances sont
		// écrites par DEUX vues du même tampon (float pour position/taille/rotation, uint pour
		// la couleur RGBA8) : NkSL n'a pas floatBitsToUint, et une structure {vec3, float, uint,
		// float} en std430 s'alignerait sur 32 o alors que le dessin en lit 24.
		// Dialecte NkSL : pas de return anticipé, corps sous `if (i < p.count)`.
		static const char *kParticlesNkSL = R"NKSL(
@binding(set=0, binding=0) buffer StateBuf { vec4 s[]; } S;
@binding(set=0, binding=1) buffer BirthBuf { vec4 b[]; } B;
@binding(set=0, binding=2) buffer InstF { float f[]; } IF;
@binding(set=0, binding=3) buffer InstU { uint u[]; } IU;
@binding(set=0, binding=4) uniform Params {
    vec4 gravityDt; vec4 colorStart; vec4 colorEnd;
    float sizeStart; float sizeEnd; uint count; uint mode;
} p;
layout(local_size_x = 256) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.count) {
        if (p.mode == 0u) {
            uint k = i * 3u;
            vec4 pl = B.b[k];
            vec4 vr = B.b[k + 1u];
            vec4 sr = B.b[k + 2u];
            uint slot = uint(sr.x) * 3u;
            S.s[slot] = pl;
            S.s[slot + 1u] = vec4(vr.x, vr.y, vr.z, pl.w);
            S.s[slot + 2u] = vec4(vr.w, sr.y, 1.0, 0.0);
        } else {
            uint k = i * 3u;
            vec4 pl = S.s[k];
            vec4 vm = S.s[k + 1u];
            vec4 ra = S.s[k + 2u];
            float dt = p.gravityDt.w;
            float life = pl.w - dt;
            uint o = i * 6u;
            if (ra.z > 0.5 && life > 0.0) {
                vec3 v = vec3(vm.x, vm.y, vm.z) + vec3(p.gravityDt.x, p.gravityDt.y, p.gravityDt.z) * dt;
                vec3 x = vec3(pl.x, pl.y, pl.z) + v * dt;
                float rot = ra.x + ra.y * dt;
                float t = 1.0 - life / vm.w;
                vec4 c = p.colorStart + (p.colorEnd - p.colorStart) * t;
                float sz = p.sizeStart + (p.sizeEnd - p.sizeStart) * t;
                S.s[k] = vec4(x.x, x.y, x.z, life);
                S.s[k + 1u] = vec4(v.x, v.y, v.z, vm.w);
                S.s[k + 2u] = vec4(rot, ra.y, 1.0, 0.0);
                IF.f[o] = x.x;
                IF.f[o + 1u] = x.y;
                IF.f[o + 2u] = x.z;
                IF.f[o + 3u] = sz;
                uint cr = uint(c.x * 255.0);
                uint cg = uint(c.y * 255.0);
                uint cb = uint(c.z * 255.0);
                uint ca = uint(c.w * 255.0);
                IU.u[o + 4u] = cr | (cg << 8u) | (cb << 16u) | (ca << 24u);
                IF.f[o + 5u] = rot;
            } else {
                S.s[k + 2u] = vec4(ra.x, ra.y, 0.0, 0.0);
                IF.f[o + 3u] = 0.0;
            }
        }
    }
}
)NKSL";

		bool NkParticleStoreGPU::CompileKernel() {
			NkString src(kParticlesNkSL);
			NkSLCompiler slc;
			NkSLCompileResult gl = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
			if (!gl.success) {
				mFail = "NkSL -> GLSL refuse";
				return false;
			}
			NkShaderConvertResult hl, sp, ms;
			NkSLCompileResult glo;
			NkShaderDesc sd;
			sd.debugName = "particles_sim";
			const NkGraphicsApi api = mDevice->GetApi();
			if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
				hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, 50u, "particles_sim");
				if (!hl.success) {
					mFail = "GLSL -> HLSL refuse";
					return false;
				}
				sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
				sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, "particles_sim");
				if (!sp.success) {
					mFail = "GLSL -> SPIR-V refuse";
					return false;
				}
				sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
			} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
				ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, "particles_sim");
				if (!ms.success) {
					mFail = "GLSL -> MSL refuse";
					return false;
				}
				sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
				glo = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
				if (!glo.success) {
					mFail = "NkSL -> GLSL(GL) refuse";
					return false;
				}
				// Instrument (05/09) : NK_VFX_DUMP=1 imprime le GLSL genere -- par fwrite, jamais par un
				// formateur a marqueurs (les accolades du shader seraient prises pour des marqueurs).
				if (const char *dump = std::getenv("NK_VFX_DUMP"); dump && dump[0] == '1') {
					std::fputs("[NkVFX] GLSL genere pour particles_sim (GL) :\n", stderr);
					std::fwrite(glo.source.CStr(), 1, glo.source.Size(), stderr);
					std::fputs("\n[NkVFX] fin du GLSL\n", stderr);
				}
				sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
			} else {
				mFail = "API sans compute";
				return false;
			}
			mShader = mDevice->CreateShader(sd);
			if (!mShader.IsValid()) {
				mFail = "CreateShader refuse (le journal du device dit pourquoi)";
				return false;
			}
			NkDescriptorSetLayoutDesc ld;
			ld.Add(0, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(1, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(2, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(3, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(4, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			mLayout = mDevice->CreateDescriptorSetLayout(ld);
			NkComputePipelineDesc cpd;
			cpd.shader = mShader;
			cpd.debugName = "particles_sim";
			cpd.descriptorSetLayouts.PushBack(mLayout);
			mPipe = mDevice->CreateComputePipeline(cpd);
			if (!mPipe.IsValid()) {
				mFail = "CreateComputePipeline refuse";
				return false;
			}
			return true;
		}

		bool NkParticleStoreGPU::Init(NkIDevice *device, const NkEmitterDesc &desc) {
			mDevice = device;
			mCapacity = desc.maxParticles;
			mFail = "";
			mAliveCount = 0;
			if (!device) {
				mFail = "pas de device";
				return false;
			}
			if (!device->GetCaps().computeShaders) {
				mFail = "pas de compute sur ce device";
				return false;
			}
			if (mCapacity == 0) {
				mFail = "capacite nulle";
				return false;
			}
			// L'horloge des vies.
			mLife.Resize(mCapacity);
			mAlive.Resize(mCapacity);
			for (uint32 i = 0; i < mCapacity; ++i) {
				mLife[i] = 0.f;
				mAlive[i] = 0;
			}
			mFree.Clear();
			mFree.Reserve(mCapacity);
			for (uint32 i = mCapacity; i > 0; --i)
				mFree.PushBack(i - 1); // l'emplacement 0 sort en premier, comme le stockage CPU
			mPending.Clear();
			mPending.Reserve(256);
			// Les tampons. L'état part à zéro (vivante = 0 partout) : un tampon non initialisé
			// porterait des drapeaux au hasard, et le noyau dessinerait des fantômes.
			{
				NkVector<uint8> zeros;
				zeros.Resize(mCapacity * 48u);
				for (uint32 i = 0; i < mCapacity * 48u; ++i)
					zeros[i] = 0;
				NkBufferDesc sd = NkBufferDesc::Storage((uint64)mCapacity * 48u);
				sd.initialData = zeros.Data();
				sd.debugName = "particles_state";
				mState = device->CreateBuffer(sd);
				NkBufferDesc bd = NkBufferDesc::Storage((uint64)mCapacity * (uint64)sizeof(GpuBirth));
				bd.debugName = "particles_births";
				mBirths = device->CreateBuffer(bd);
				// Le tampon d'instances : STORAGE pour le noyau, VERTEX pour le dessin -- le même.
				NkBufferDesc id = NkBufferDesc::Storage((uint64)mCapacity * (uint64)sizeof(NkParticleInstance));
				id.bindFlags = id.bindFlags | NkBindFlags::NK_VERTEX_BUFFER;
				id.initialData = zeros.Data(); // 24 o par emplacement <= 48 : taille 0 partout avant le premier dispatch
				id.debugName = "particles_instances";
				mInstances = device->CreateBuffer(id);
				mParamsBirth = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(Params)));
				mParamsSim = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(Params)));
			}
			if (!mState.IsValid() || !mBirths.IsValid() || !mInstances.IsValid() || !mParamsBirth.IsValid() ||
				!mParamsSim.IsValid()) {
				mFail = "un tampon de stockage n'a pas pu etre cree";
				return false;
			}
			if (!CompileKernel())
				return false;
			// Deux jeux de descripteurs, un par mode (les mêmes tampons, un UBO différent).
			auto bindSet = [&](NkDescSetHandle set, NkBufferHandle params) {
				NkDescriptorWrite w{};
				w.set = set;
				w.type = NkDescriptorType::NK_STORAGE_BUFFER;
				w.binding = 0;
				w.buffer = mState;
				mDevice->UpdateDescriptorSets(&w, 1);
				w.binding = 1;
				w.buffer = mBirths;
				mDevice->UpdateDescriptorSets(&w, 1);
				w.binding = 2;
				w.buffer = mInstances;
				mDevice->UpdateDescriptorSets(&w, 1);
				w.binding = 3;
				w.buffer = mInstances;
				mDevice->UpdateDescriptorSets(&w, 1);
				mDevice->BindUniformBuffer(set, 4, params);
			};
			mSetBirth = mDevice->AllocateDescriptorSet(mLayout);
			mSetSim = mDevice->AllocateDescriptorSet(mLayout);
			if (!mSetBirth.IsValid() || !mSetSim.IsValid()) {
				mFail = "AllocateDescriptorSet refuse";
				return false;
			}
			bindSet(mSetBirth, mParamsBirth);
			bindSet(mSetSim, mParamsSim);
			mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			if (!mCmd) {
				mFail = "CreateCommandBuffer(compute) refuse";
				return false;
			}
			return true;
		}

		void NkParticleStoreGPU::Shutdown(NkIDevice *device) {
			if (device) {
				if (mCmd)
					device->DestroyCommandBuffer(mCmd);
				if (mSetBirth.IsValid())
					device->FreeDescriptorSet(mSetBirth);
				if (mSetSim.IsValid())
					device->FreeDescriptorSet(mSetSim);
				NkBufferHandle *bufs[] = {&mState, &mBirths, &mInstances, &mParamsBirth, &mParamsSim};
				for (NkBufferHandle *b : bufs)
					if (b->IsValid()) {
						device->DestroyBuffer(*b);
						*b = {};
					}
				// Shader et pipeline : détruits avec le device (même choix que NkIBLCompute).
			}
			mCmd = nullptr;
			mSetBirth = {};
			mSetSim = {};
			mDevice = nullptr;
		}

		void NkParticleStoreGPU::Spawn(const NkParticleBirth *births, uint32 n) {
			for (uint32 k = 0; k < n; ++k) {
				if (mFree.Empty())
					return; // plein : la naissance est perdue, comme le stockage CPU
				const uint32 i = mFree.Back();
				mFree.PopBack();
				mAlive[i] = 1;
				mLife[i] = births[k].life;
				GpuBirth gb;
				gb.posLife = {births[k].pos.x, births[k].pos.y, births[k].pos.z, births[k].life};
				gb.velRot = {births[k].vel.x, births[k].vel.y, births[k].vel.z, births[k].rotation};
				gb.slotRotSpeed = {(float32)i, births[k].rotSpeed, 0.f, 0.f};
				mPending.PushBack(gb);
			}
		}

		void NkParticleStoreGPU::Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
									  NkParticleStepStats &stats) {
			(void)cmd;
			const int64 t1 = ::nkentseu::NkChrono::Now().nanoseconds;
			// 1) L'horloge des vies -- les MÊMES opérations que le noyau (life - dt en float32,
			//    morte si <= 0), donc le même verdict, emplacement par emplacement.
			mAliveCount = 0;
			float32 *L = mLife.Data();
			uint8 *A = mAlive.Data();
			for (uint32 i = 0; i < mCapacity; ++i) {
				if (!A[i])
					continue;
				L[i] -= dt;
				if (L[i] <= 0.f) {
					A[i] = 0;
					mFree.PushBack(i);
					continue;
				}
				++mAliveCount;
			}
			// 2) Naissances de l'image et paramètres des deux modes.
			const uint32 nb = (uint32)mPending.Size();
			Params pb;
			pb.gravityDt = {desc.gravity.x, desc.gravity.y, desc.gravity.z, dt};
			pb.colorStart = desc.colorStart;
			pb.colorEnd = desc.colorEnd;
			pb.sizeStart = desc.sizeStart;
			pb.sizeEnd = desc.sizeEnd;
			pb.count = nb;
			pb.mode = 0;
			Params ps = pb;
			ps.count = mCapacity;
			ps.mode = 1;
			uint32 bytes = 0;
			if (nb > 0) {
				mDevice->WriteBuffer(mBirths, mPending.Data(), (uint64)nb * sizeof(GpuBirth));
				bytes += nb * (uint32)sizeof(GpuBirth);
			}
			mDevice->WriteBuffer(mParamsBirth, &pb, sizeof(Params));
			mDevice->WriteBuffer(mParamsSim, &ps, sizeof(Params));
			bytes += 2u * (uint32)sizeof(Params);
			const int64 t2 = ::nkentseu::NkChrono::Now().nanoseconds;
			// 3) Deux dispatchs, puis la barrière vers le dessin (le tampon d'instances est lu
			//    comme attribut de sommet : sur GL, l'état VERTEX_BUFFER donne ALL_BARRIER_BITS).
			mCmd->Reset();
			mCmd->Begin();
			mCmd->BindComputePipeline(mPipe);
			if (nb > 0) {
				mCmd->BindDescriptorSet(mSetBirth, 0);
				mCmd->Dispatch((nb + 255u) / 256u, 1, 1);
				mCmd->UAVBarrier(mState);
			}
			mCmd->BindDescriptorSet(mSetSim, 0);
			mCmd->Dispatch((mCapacity + 255u) / 256u, 1, 1);
			{
				NkBufferBarrier b{mInstances, NkResourceState::NK_UNORDERED_ACCESS, NkResourceState::NK_VERTEX_BUFFER};
				mCmd->Barrier(&b, 1, nullptr, 0);
			}
			mCmd->End();
			mDevice->Submit(&mCmd, 1);
			mPending.Clear();
			const int64 t3 = ::nkentseu::NkChrono::Now().nanoseconds;
			stats.simMs += (float32)((t2 - t1) / 1.0e6);	// horloge des vies + naissances (CPU)
			stats.uploadMs += (float32)((t3 - t2) / 1.0e6); // enregistrement + soumission des dispatchs
			stats.uploadBytes += bytes;
			stats.alive += mAliveCount;
		}

	} // namespace renderer
} // namespace nkentseu
