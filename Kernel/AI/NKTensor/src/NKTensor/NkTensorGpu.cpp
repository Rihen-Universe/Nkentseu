// =============================================================================
// NkTensorGpu.cpp — implémentation du contexte GPU de NKTensor.
// Réutilise le chemin compute PROUVÉ (cf. Applications/NkComputeNkSL) :
//   NkSL -> GLSL-Vulkan -> (glslang/SPIRV-Cross) -> HLSL/SPIRV/MSL -> pipeline compute.
// =============================================================================
#include "NKTensor/NkTensorGpu.h"
#include "NKTensor/NkTensor.h" // pour ToGPU/ToCPU + NkTensorInternal (construction GPU)

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace ai {

		// ---- État interne (pimpl) : tout NKRHI/NKSL confiné ici -----------------
		struct NkTensorGpu::Impl {
				NkIDevice *device = nullptr;
				bool tried = false;
				const char *backend = "none";

				NkUnorderedMap<uint64, NkBufferHandle> buffers; // id opaque -> handle
				uint64 nextId = 1;

				struct Kernel {
						NkString name;
						NkShaderHandle shader;
						NkPipelineHandle pipe;
						NkDescSetHandle layout;
						NkBufferHandle params; // UBO 16o persistant (pas de churn par dispatch)
				};

				NkVector<Kernel> kernels; // cache par nom (peu d'entrées -> linéaire)

				// Compile (ou récupère du cache) un kernel NkSL compute.
				// nBuffers storage buffers (bindings 0..n-1) + 1 UBO au binding uboBinding.
				Kernel *GetOrCompile(const char *name, const NkString &nksl, uint32 nBuffers, uint32 uboBinding) {
					for (uint32 i = 0; i < kernels.Size(); i++)
						if (kernels[i].name == name)
							return &kernels[i];

					NkSLCompiler slc;
					NkSLCompileResult gl = slc.Compile(nksl, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
					if (!gl.success) {
						logger_src.Errorf("[NkTensorGpu] NkSL->GLSL KO (%s)\n", name);
						return nullptr;
					}

					// GLSL-Vulkan -> source du backend courant.
					// IMPORTANT : NkShaderStageDesc.{hlsl,glsl,msl}Source sont des `const char*`
					// NON possédés — la source doit rester VIVANTE jusqu'après CreateShader. On
					// hisse donc les holders (hl/sp/ms/glo) hors des branches `if` : sinon ils
					// sont détruits à la fermeture de leur bloc -> pointeur dangling lu par
					// CreateShader (mémoire libérée éventuellement réallouée -> source corrompue).
					NkShaderConvertResult hl, sp, ms;
					NkSLCompileResult glo;
					NkShaderDesc sd;
					sd.debugName = name;
					const NkGraphicsApi api = device->GetApi();
					if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
						// SM5.0 pour DX11 ET DX12 : le device DX12 retombe sur fxc (cs_5_1) —
						// chemin prouvé (dxc SM6 a un bug d'encodage source à corriger à part).
						const uint32 sm = 50u;
						hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, sm, name);
						if (!hl.success) {
							logger_src.Errorf("[NkTensorGpu] GLSL->HLSL KO (%s): %s\n", name, hl.errors.CStr());
							return nullptr;
						}
						sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
					} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
						sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, name);
						if (!sp.success) {
							logger_src.Errorf("[NkTensorGpu] GLSL->SPIRV KO (%s)\n", name);
							return nullptr;
						}
						sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
					} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
						ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, name);
						if (!ms.success) {
							logger_src.Errorf("[NkTensorGpu] GLSL->MSL KO (%s)\n", name);
							return nullptr;
						}
						sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
					} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
						glo = slc.Compile(nksl, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
						if (!glo.success) {
							logger_src.Errorf("[NkTensorGpu] NkSL->GLSL(GL) KO (%s)\n", name);
							return nullptr;
						}
						sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
					} else {
						logger_src.Errorf("[NkTensorGpu] API compute non supportée (%s)\n", name);
						return nullptr;
					}

					NkShaderHandle sh = device->CreateShader(sd);
					if (!sh.IsValid()) {
						logger_src.Errorf("[NkTensorGpu] CreateShader KO (%s)\n", name);
						return nullptr;
					}

					// Layout D'ABORD : le pipeline Vulkan en a besoin (setLayouts).
					NkDescriptorSetLayoutDesc ld;
					for (uint32 i = 0; i < nBuffers; i++)
						ld.Add(i, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
					ld.Add(uboBinding, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
					NkDescSetHandle layout = device->CreateDescriptorSetLayout(ld);

					NkComputePipelineDesc cpd;
					cpd.shader = sh;
					cpd.debugName = name;
					cpd.descriptorSetLayouts.PushBack(layout); // requis par Vulkan
					NkPipelineHandle pipe = device->CreateComputePipeline(cpd);
					if (!pipe.IsValid()) {
						logger_src.Errorf("[NkTensorGpu] Pipeline KO (%s)\n", name);
						return nullptr;
					}

					Kernel k;
					k.name = name;
					k.shader = sh;
					k.pipe = pipe;
					k.layout = layout;
					// UBO persistant dimensionné pour le PLUS GROS bloc de params (gather = 80 o :
					// rank/count/offset/pad + 4 uvec4). Les kernels qui écrivent moins (4/12/16 o)
					// font une écriture partielle, ce qui est valide.
					k.params = device->CreateBuffer(NkBufferDesc::Uniform(256)); // persistant
					kernels.PushBack(k);
					logger_src.Infof("[NkTensorGpu] kernel '%s' compilé (%s)\n", name, NkGraphicsApiName(api));
					return &kernels[kernels.Size() - 1];
				}

				NkBufferHandle Handle(uint64 id) {
					auto *h = buffers.Find(id);
					return h ? *h : NkBufferHandle{};
				}
		};

		// ---- Cycle de vie -------------------------------------------------------
		NkTensorGpu &NkTensorGpu::Get() {
			static NkTensorGpu inst;
			return inst;
		}

		NkTensorGpu::~NkTensorGpu() {
			Shutdown();
		}

		bool NkTensorGpu::EnsureInit() {
			if (!mImpl)
				mImpl = new Impl();
			if (mImpl->tried)
				return mImpl->device != nullptr;
			mImpl->tried = true;

			// Device compute headless. Ordre par FIABILITÉ compute vérifiée sur NVIDIA :
			// les 4 backends (Vulkan, OpenGL, DX11, DX12) sont désormais VALIDÉS (compute
			// NkSL headless = résultat exact, 4/4). Vulkan reste préféré (le plus robuste).
			// Override diagnostic : NK_TENSOR_API=vulkan|opengl|dx11|dx12|metal force un
			// backend précis (utile pour valider/reproduire un backend donné).
			NkGraphicsApi forced = (NkGraphicsApi)0;
			bool hasForced = false;
			if (const char *e = getenv("NK_TENSOR_API")) {
				NkString s(e);
				if (s == NkString("vulkan")) {
					forced = NkGraphicsApi::NK_GFX_API_VULKAN;
					hasForced = true;
				} else if (s == NkString("opengl")) {
					forced = NkGraphicsApi::NK_GFX_API_OPENGL;
					hasForced = true;
				} else if (s == NkString("dx11")) {
					forced = NkGraphicsApi::NK_GFX_API_DX11;
					hasForced = true;
				} else if (s == NkString("dx12")) {
					forced = NkGraphicsApi::NK_GFX_API_DX12;
					hasForced = true;
				} else if (s == NkString("metal")) {
					forced = NkGraphicsApi::NK_GFX_API_METAL;
					hasForced = true;
				}
			}
			const NkGraphicsApi tryOrderAll[] = {
				NkGraphicsApi::NK_GFX_API_VULKAN,
				NkGraphicsApi::NK_GFX_API_METAL, // Apple
				NkGraphicsApi::NK_GFX_API_OPENGL, NkGraphicsApi::NK_GFX_API_DX11, NkGraphicsApi::NK_GFX_API_DX12,
			};
			const NkGraphicsApi one[1] = {forced};
			const NkGraphicsApi *tryOrder = hasForced ? one : tryOrderAll;
			const nk_size tryCount = hasForced ? 1 : (nk_size)(sizeof(tryOrderAll) / sizeof(tryOrderAll[0]));
			for (nk_size ti = 0; ti < tryCount; ++ti) {
				NkGraphicsApi api = tryOrder[ti];
				NkDeviceInitInfo di;
				di.api = api; // pas de surface -> headless
				di.context.software.threading = true;
				NkIDevice *dev = NkDeviceFactory::Create(di);
				if (dev && dev->IsValid() && dev->GetCaps().computeShaders) {
					mImpl->device = dev;
					mImpl->backend = NkGraphicsApiName(api);
					logger_src.Infof("[NkTensorGpu] device compute: %s\n", mImpl->backend);
					return true;
				}
				if (dev)
					NkDeviceFactory::Destroy(dev);
			}
			logger_src.Infof("[NkTensorGpu] aucun device compute GPU disponible (CPU only)\n");
			return false;
		}

		void NkTensorGpu::Shutdown() {
			if (!mImpl)
				return;
			if (mImpl->device) {
				for (uint32 i = 0; i < mImpl->kernels.Size(); i++) {
					mImpl->device->DestroyPipeline(mImpl->kernels[i].pipe);
					mImpl->device->DestroyShader(mImpl->kernels[i].shader);
					mImpl->device->DestroyDescriptorSetLayout(mImpl->kernels[i].layout);
					if (mImpl->kernels[i].params.IsValid())
						mImpl->device->DestroyBuffer(mImpl->kernels[i].params);
				}
				mImpl->buffers.ForEach([this](const uint64 &, NkBufferHandle &h) { mImpl->device->DestroyBuffer(h); });
				NkDeviceFactory::Destroy(mImpl->device);
				mImpl->device = nullptr;
			}
			delete mImpl;
			mImpl = nullptr;
		}

		bool NkTensorGpu::IsAvailable() {
			return EnsureInit();
		}

		const char *NkTensorGpu::BackendName() {
			EnsureInit();
			return mImpl ? mImpl->backend : "none";
		}

		// ---- Défauts GPU ---------------------------------------------------------
		// POURQUOI CE COMPTEUR. Quand une allocation échoue ou qu'un tampon est
		// invalide, tout ce code se contentait d'un `return false` que personne ne
		// regarde : le calcul n'a pas lieu, et l'entraînement continue comme si de
		// rien n'était. Constaté le 2026-08-09 sur un lot trop grand — la perte
		// restait EXACTEMENT à ln(vocabulaire) pendant que le run paraissait 3,6×
		// plus rapide, parce qu'il ne faisait rien. Quatre heures auraient pu y
		// passer sans un seul message.
		// Désormais : chaque défaut est journalisé et compté, et l'appelant peut
		// interroger le compteur pour s'arrêter au lieu de brasser du vide.
		static int64 gGpuDefauts = 0;
		static int64 gGpuDefautsJournalises = 0;

		void NkGpuSignalerDefaut(const char *ou, const char *quoi, int64 valeur) {
			++gGpuDefauts;
			// On ne noie pas le journal : les 12 premiers suffisent à identifier
			// l'opération fautive, le compteur dit le reste.
			if (gGpuDefautsJournalises < 12) {
				++gGpuDefautsJournalises;
				logger.Info("[NkTensorGpu] DEFAUT dans '{0}' : {1} ({2}). Le calcul n'a PAS eu lieu — "
							"l'entrainement continuerait sur des valeurs inchangees.",
							ou, quoi, (long long)valeur);
			}
		}

		int64 NkTensorGpu::DefautCount() {
			return gGpuDefauts;
		}

		// ---- Buffers ------------------------------------------------------------
		uint64 NkTensorGpu::CreateBuffer(nk_size bytes) {
			if (!EnsureInit())
				return 0;
			NkBufferHandle h = mImpl->device->CreateBuffer(NkBufferDesc::Storage(bytes, false));
			if (!h.IsValid()) {
				NkGpuSignalerDefaut("CreateBuffer", "allocation refusee, octets demandes", (int64)bytes);
				return 0;
			}
			uint64 id = mImpl->nextId++;
			mImpl->buffers.Insert(id, h);
			return id;
		}

		void NkTensorGpu::DestroyBuffer(uint64 id) {
			if (!mImpl || !mImpl->device || id == 0)
				return;
			auto *h = mImpl->buffers.Find(id);
			if (h) {
				mImpl->device->DestroyBuffer(*h);
				mImpl->buffers.Erase(id);
			}
		}

		bool NkTensorGpu::Upload(uint64 id, const void *data, nk_size bytes) {
			if (!mImpl || !mImpl->device)
				return false;
			NkBufferHandle h = mImpl->Handle(id);
			if (!h.IsValid())
				return false;
			return mImpl->device->WriteBuffer(h, data, bytes);
		}

		bool NkTensorGpu::Download(uint64 id, void *out, nk_size bytes) {
			if (!mImpl || !mImpl->device)
				return false;
			NkBufferHandle h = mImpl->Handle(id);
			if (!h.IsValid())
				return false;
			return mImpl->device->ReadBuffer(h, out, bytes);
		}

		// ---- Dispatch helpers ---------------------------------------------------
		static void BindSSBO(NkIDevice *dev, NkDescSetHandle set, uint32 binding, NkBufferHandle buf) {
			NkDescriptorWrite w{};
			w.set = set;
			w.binding = binding;
			w.type = NkDescriptorType::NK_STORAGE_BUFFER;
			w.buffer = buf;
			dev->UpdateDescriptorSets(&w, 1);
		}

		bool NkTensorGpu::RunBinary(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint64 c,
									uint32 count) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/ 3, /*ubo*/ 3);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(a), hb = d->Handle(b), hc = d->Handle(c);
			if (!ha.IsValid() || !hb.IsValid() || !hc.IsValid()) {
				NkGpuSignalerDefaut(name, "tampon invalide (allocation refusee en amont)", (int64)count);
				return false;
			}

			struct P {
					uint32 count;
			} p{count};

			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			BindSSBO(d->device, set, 2, hc);
			d->device->BindUniformBuffer(set, 3, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hc);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle(); // flush avant le Download (ReadBuffer synchronise aussi via Map)

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		bool NkTensorGpu::RunUnary(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint32 count) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/ 2, /*ubo*/ 2);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(a), hb = d->Handle(b);
			if (!ha.IsValid() || !hb.IsValid()) {
				NkGpuSignalerDefaut(name, "tampon invalide (allocation refusee en amont)", (int64)count);
				return false;
			}

			struct P {
					uint32 count;
			} p{count};

			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			d->device->BindUniformBuffer(set, 2, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hb);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle(); // flush avant le Download (ReadBuffer synchronise aussi via Map)

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		bool NkTensorGpu::RunUnaryScalar(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint32 count,
										 float s) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/ 2, /*ubo*/ 2);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(a), hb = d->Handle(b);
			if (!ha.IsValid() || !hb.IsValid()) {
				NkGpuSignalerDefaut(name, "tampon invalide (allocation refusee en amont)", (int64)count);
				return false;
			}

			struct P {
					uint32 count;
					float s;
			} p{count, s};

			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			d->device->BindUniformBuffer(set, 2, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hb);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// Réduction segmentée : buffers 0,1 (A,B) + UBO { uint outer,reduce,inner } binding 2.
		// Un thread par élément de sortie (outer*inner threads).
		bool NkTensorGpu::RunReduce(const char *name, const NkString &nkslSrc, uint64 a, uint64 out, uint32 outer,
									uint32 reduce, uint32 inner) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/ 2, /*ubo*/ 2);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(a), hb = d->Handle(out);
			if (!ha.IsValid() || !hb.IsValid()) {
				NkGpuSignalerDefaut(name, "tampon invalide (allocation refusee en amont)", (int64)(outer));
				return false;
			}

			struct P {
					uint32 outer, reduce, inner, pad;
			} p{outer, reduce, inner, 0};

			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			d->device->BindUniformBuffer(set, 2, k->params);

			const uint32 total = outer * inner;
			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((total + 63) / 64, 1, 1);
			cmd->UAVBarrier(hb);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// Gather par strides : buffers 0,1 (A,B) + UBO { rank,count,offset,pad, uvec4 shp0,
		// shp1, str0, str1 } binding 2. Un thread par élément de sortie.
		static const char *kGatherNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Meta {
    uint rank; uint count; uint offset; uint pad0;
    uvec4 shp0; uvec4 shp1;   // formes des dims 0..7
    uvec4 str0; uvec4 str1;   // strides des dims 0..7 (en éléments)
} m;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < m.count) {
        uint rem = idx;
        uint src = m.offset;
        for (uint dd = 0u; dd < m.rank; dd = dd + 1u) {
            uint d  = m.rank - 1u - dd;
            uint sz; uint st;
            if (d < 4u) { sz = m.shp0[d];      st = m.str0[d];      }
            else        { sz = m.shp1[d - 4u]; st = m.str1[d - 4u]; }
            uint coord = rem % sz;
            rem = rem / sz;
            src = src + coord * st;
        }
        B.data[idx] = A.data[src];
    }
}
)NKSL";

		bool NkTensorGpu::RunGather(uint64 in, uint64 out, uint32 rank, uint32 offset, const uint32 *shape,
									const uint32 *strides, uint32 count) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile("gather", NkString(kGatherNkSL), /*nBuffers*/ 2, /*ubo*/ 2);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(in), hb = d->Handle(out);
			if (!ha.IsValid() || !hb.IsValid()) {
				NkGpuSignalerDefaut("gather", "tampon invalide (allocation refusee en amont)", (int64)(rank));
				return false;
			}

			struct Meta {
					uint32 rank, count, offset, pad0;
					uint32 shp[8];
					uint32 str[8];
			} meta{};

			meta.rank = rank;
			meta.count = count;
			meta.offset = offset;
			for (uint32 i = 0; i < 8; i++) {
				meta.shp[i] = (i < rank) ? shape[i] : 1;
				meta.str[i] = (i < rank) ? strides[i] : 0;
			}
			d->device->WriteBuffer(k->params, &meta, sizeof(meta));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			d->device->BindUniformBuffer(set, 2, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hb);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// im2col / col2im : buffers 0,1 (A,B) + UBO { 12 uints } binding 2.
		bool NkTensorGpu::RunConvOp(const char *name, const NkString &nkslSrc, uint64 in, uint64 out, const uint32 *p12,
									uint32 count) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/ 2, /*ubo*/ 2);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(in), hb = d->Handle(out);
			if (!ha.IsValid() || !hb.IsValid()) {
				NkGpuSignalerDefaut(name, "tampon invalide (allocation refusee en amont)", (int64)(0));
				return false;
			}

			struct P {
					uint32 v[12];
			} p{};

			for (int i = 0; i < 12; i++)
				p.v[i] = p12[i];
			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			d->device->BindUniformBuffer(set, 2, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hb);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// Générique 3 buffers (a,b,c) + UBO {12 uints} binding 3.
		bool NkTensorGpu::RunOp3(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint64 c,
								 const uint32 *p12, uint32 count) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/ 3, /*ubo*/ 3);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(a), hb = d->Handle(b), hc = d->Handle(c);
			if (!ha.IsValid() || !hb.IsValid() || !hc.IsValid()) {
				NkGpuSignalerDefaut(name, "tampon invalide (allocation refusee en amont)", (int64)count);
				return false;
			}

			struct P {
					uint32 v[12];
			} p{};

			for (int i = 0; i < 12; i++)
				p.v[i] = p12[i];
			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			BindSSBO(d->device, set, 2, hc);
			d->device->BindUniformBuffer(set, 3, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hc);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// Pas d'Adam fusé : buffers 0,1,2,3 (param,grad,m,v) + UBO binding 4. Tout en place.
		bool NkTensorGpu::RunAdam(uint64 param, uint64 grad, uint64 m, uint64 v, uint32 count, float lr, float b1,
								  float b2, float eps, float b1t, float b2t, float wd) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			static const char *kAdamNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufG { float data[]; } G;
@binding(set=0, binding=2) buffer BufM { float data[]; } M;
@binding(set=0, binding=3) buffer BufV { float data[]; } V;
@binding(set=0, binding=4) uniform Params {
    uint count; float lr; float b1; float b2; float eps; float b1t; float b2t; float wd;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        float g  = G.data[i];
        float mi = pc.b1 * M.data[i] + (1.0 - pc.b1) * g;
        float vi = pc.b2 * V.data[i] + (1.0 - pc.b2) * g * g;
        M.data[i] = mi;
        V.data[i] = vi;
        float mhat = mi / pc.b1t;
        float vhat = vi / pc.b2t;
        // AdamW : weight decay découplé (pc.wd = 0 -> Adam classique).
        P.data[i] = P.data[i] - pc.lr * (mhat / (sqrt(vhat) + pc.eps) + pc.wd * P.data[i]);
    }
}
)NKSL";
			Impl::Kernel *k = d->GetOrCompile("adam", NkString(kAdamNkSL), /*nBuffers*/ 4, /*ubo*/ 4);
			if (!k)
				return false;
			NkBufferHandle hp = d->Handle(param), hg = d->Handle(grad), hm = d->Handle(m), hv = d->Handle(v);
			if (!hp.IsValid() || !hg.IsValid() || !hm.IsValid() || !hv.IsValid())
				return false;

			struct P {
					uint32 count;
					float lr, b1, b2, eps, b1t, b2t, wd;
			} p{count, lr, b1, b2, eps, b1t, b2t, wd};

			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, hp);
			BindSSBO(d->device, set, 1, hg);
			BindSSBO(d->device, set, 2, hm);
			BindSSBO(d->device, set, 3, hv);
			d->device->BindUniformBuffer(set, 4, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63) / 64, 1, 1);
			cmd->UAVBarrier(hp);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// MatMul : buffers 0,1,2 (A,B,C) + UBO { uint M,N,K } binding 3.
		// Dispatch 1D (index plat) : chaque thread calcule un élément C[idx]. On
		// évite le workgroup 2D (course intermittente observée sur WARP headless).
		static const char *kMatMulNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Dims { uint M; uint N; uint K; } d;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < d.M * d.N) {
        uint row = idx / d.N;
        uint col = idx - row * d.N;
        float acc = 0.0;
        for (uint k = 0u; k < d.K; k = k + 1u) {
            acc = acc + A.data[row * d.K + k] * B.data[k * d.N + col];
        }
        C.data[idx] = acc;
    }
}
)NKSL";

		bool NkTensorGpu::RunMatMul(uint64 a, uint64 b, uint64 c, uint32 M, uint32 N, uint32 K) {
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			Impl::Kernel *k = d->GetOrCompile("matmul", NkString(kMatMulNkSL), /*nBuffers*/ 3, /*ubo*/ 3);
			if (!k)
				return false;
			NkBufferHandle ha = d->Handle(a), hb = d->Handle(b), hc = d->Handle(c);
			if (!ha.IsValid() || !hb.IsValid() || !hc.IsValid()) {
				NkGpuSignalerDefaut("matmul", "tampon invalide (allocation refusee en amont)", (int64)(M * N));
				return false;
			}

			struct P {
					uint32 M, N, K, pad;
			} p{M, N, K, 0};

			d->device->WriteBuffer(k->params, &p, sizeof(p));

			NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
			BindSSBO(d->device, set, 0, ha);
			BindSSBO(d->device, set, 1, hb);
			BindSSBO(d->device, set, 2, hc);
			d->device->BindUniformBuffer(set, 3, k->params);

			auto *cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(k->pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((M * N + 63) / 64, 1, 1); // 1D : un thread par élément C
			cmd->UAVBarrier(hc);
			cmd->End();
			d->device->Submit(&cmd, 1);
			d->device->WaitIdle(); // flush avant le Download (ReadBuffer synchronise aussi via Map)

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// =====================================================================
		// Intégration au niveau tenseur : construction GPU + transferts CPU<->GPU.
		// NkTensorInternal est ami de NkTensor -> accès aux membres privés.
		// =====================================================================
		struct NkTensorInternal {
				static NkTensor MakeGpu(const NkShape &shape, NkDType dtype, uint64 gpuBuf) {
					NkTensor t;
					t.mStorage = NkTensorStorage::Allocate(0); // pas de data CPU
					t.mStorage->gpuBuffer = gpuBuf;
					t.mShape = shape;
					t.mStrides = NkContiguousStrides(shape);
					t.mDType = dtype;
					t.mDevice = NkDevice::NK_GPU;
					t.mOffset = 0;
					return t;
				}

				static uint64 GpuBuffer(const NkTensor &t) {
					return t.mStorage ? t.mStorage->gpuBuffer : 0;
				}

				static int64 Offset(const NkTensor &t) {
					return t.mOffset;
				}
		};

		// Kernel élémentaire add (mêmes bindings que RunBinary attend).
		static const char *kAddNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) { C.data[i] = A.data[i] + B.data[i]; }
}
)NKSL";

		NkTensor NkGpuAdd(const NkTensor &a, const NkTensor &b) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			NkTensor gb = (b.Device() == NkDevice::NK_GPU) ? b : b.ToGPU();
			if (!ga.IsValid() || !gb.IsValid())
				return NkTensor{};
			if (ga.Numel() != gb.Numel())
				return NkTensor{}; // v1 : mêmes formes (pas de broadcast GPU)
			const int64 n = ga.Numel();
			const nk_size bytes = (nk_size)n * NkDTypeSize(ga.DType());
			uint64 cbuf = NkTensorGpu::Get().CreateBuffer(bytes);
			if (!cbuf)
				return NkTensor{};
			NkTensorGpu::Get().RunBinary("add", NkString(kAddNkSL), NkTensorInternal::GpuBuffer(ga),
										 NkTensorInternal::GpuBuffer(gb), cbuf, (uint32)n);
			return NkTensorInternal::MakeGpu(ga.Shape(), ga.DType(), cbuf);
		}

		NkTensor NkGpuMatmul(const NkTensor &a, const NkTensor &b) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			NkTensor gb = (b.Device() == NkDevice::NK_GPU) ? b : b.ToGPU();
			if (!ga.IsValid() || !gb.IsValid())
				return NkTensor{};
			if (ga.Rank() != 2 || gb.Rank() != 2)
				return NkTensor{};
			const int64 M = ga.Shape()[0], K = ga.Shape()[1];
			const int64 K2 = gb.Shape()[0], N = gb.Shape()[1];
			if (K != K2)
				return NkTensor{};
			NkShape outShape;
			outShape.PushBack(M);
			outShape.PushBack(N);
			const nk_size bytes = (nk_size)(M * N) * NkDTypeSize(ga.DType());
			uint64 cbuf = NkTensorGpu::Get().CreateBuffer(bytes);
			if (!cbuf)
				return NkTensor{};
			NkTensorGpu::Get().RunMatMul(NkTensorInternal::GpuBuffer(ga), NkTensorInternal::GpuBuffer(gb), cbuf,
										 (uint32)M, (uint32)N, (uint32)K);
			return NkTensorInternal::MakeGpu(outShape, ga.DType(), cbuf);
		}

		// Matmul par lots : a[batch,M,K] · b[batch,K,N] -> [batch,M,N]. UBO {batch,M,N,K}.
		static const char *kBatchedMatmulNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Dims { uint batch; uint M; uint N; uint K; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint total = d.batch * d.M * d.N;
    if (i < total) {
        uint col = i % d.N; uint t = i / d.N;
        uint row = t % d.M; uint bi = t / d.M;
        uint aBase = (bi * d.M + row) * d.K;
        uint bBase = bi * d.K * d.N;
        float acc = 0.0;
        for (uint k = 0u; k < d.K; k = k + 1u) acc = acc + A.data[aBase + k] * B.data[bBase + k * d.N + col];
        C.data[i] = acc;
    }
}
)NKSL";

		NkTensor NkGpuBatchedMatmul(const NkTensor &a, const NkTensor &b) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			NkTensor gb = (b.Device() == NkDevice::NK_GPU) ? b : b.ToGPU();
			if (!ga.IsValid() || !gb.IsValid() || ga.Rank() != 3 || gb.Rank() != 3)
				return NkTensor{};
			const int64 batch = ga.Shape()[0], M = ga.Shape()[1], K = ga.Shape()[2];
			const int64 N = gb.Shape()[2];
			if (gb.Shape()[0] != batch || gb.Shape()[1] != K)
				return NkTensor{};
			const int64 no = batch * M * N;
			uint64 cbuf = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(ga.DType()));
			if (!cbuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)batch, (uint32)M, (uint32)N, (uint32)K, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("bmatmul", NkString(kBatchedMatmulNkSL), NkTensorInternal::GpuBuffer(ga),
									  NkTensorInternal::GpuBuffer(gb), cbuf, p, (uint32)no);
			return NkTensorInternal::MakeGpu(NkShape{batch, M, N}, ga.DType(), cbuf);
		}

		// ---- Broadcast vec[C] sur le dernier axe de big[..,C] : biais / affine (résident) ----
		static const char *kAddBcastNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } Bv;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform P { uint count; uint cols; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < d.count) { C.data[i] = A.data[i] + Bv.data[i % d.cols]; } }
)NKSL";
		static const char *kMulBcastNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } Bv;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform P { uint count; uint cols; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < d.count) { C.data[i] = A.data[i] * Bv.data[i % d.cols]; } }
)NKSL";

		static NkTensor GpuBroadcastRow(const char *name, const char *src, const NkTensor &big, const NkTensor &vec) {
			NkTensor gb = (big.Device() == NkDevice::NK_GPU) ? big : big.ToGPU();
			NkTensor gv = (vec.Device() == NkDevice::NK_GPU) ? vec : vec.ToGPU();
			if (!gb.IsValid() || !gv.IsValid())
				return NkTensor{};
			const int64 count = gb.Numel();
			const int64 C = gv.Numel();
			if (C <= 0 || (count % C) != 0)
				return NkTensor{};
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)count * NkDTypeSize(gb.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)count, (uint32)C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3(name, NkString(src), NkTensorInternal::GpuBuffer(gb),
									  NkTensorInternal::GpuBuffer(gv), ob, p, (uint32)count);
			return NkTensorInternal::MakeGpu(gb.Shape(), gb.DType(), ob);
		}

		NkTensor NkGpuAddBroadcastRow(const NkTensor &big, const NkTensor &vec) {
			return GpuBroadcastRow("addbcast", kAddBcastNkSL, big, vec);
		}

		NkTensor NkGpuMulBroadcastRow(const NkTensor &big, const NkTensor &vec) {
			return GpuBroadcastRow("mulbcast", kMulBcastNkSL, big, vec);
		}

		// ---- Ops élémentaires GPU supplémentaires (résidence : opèrent sur des
		//      tenseurs déjà sur GPU et renvoient un tenseur GPU -> pas de transfert). ----
		static const char *kMulNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { C.data[i] = A.data[i] * B.data[i]; } }
)NKSL";
		static const char *kSubNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { C.data[i] = A.data[i] - B.data[i]; } }
)NKSL";
		static const char *kReluNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { float v = A.data[i]; B.data[i] = v > 0.0 ? v : 0.0; } }
)NKSL";
		static const char *kSigmoidNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = 1.0 / (1.0 + exp(-A.data[i])); } }
)NKSL";
		static const char *kTanhNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = tanh(A.data[i]); } }
)NKSL";

		static NkTensor GpuBinaryOp(const char *name, const char *src, const NkTensor &a, const NkTensor &b) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			NkTensor gb = (b.Device() == NkDevice::NK_GPU) ? b : b.ToGPU();
			if (!ga.IsValid() || !gb.IsValid() || ga.Numel() != gb.Numel())
				return NkTensor{};
			const int64 n = ga.Numel();
			uint64 cbuf = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(ga.DType()));
			if (!cbuf)
				return NkTensor{};
			NkTensorGpu::Get().RunBinary(name, NkString(src), NkTensorInternal::GpuBuffer(ga),
										 NkTensorInternal::GpuBuffer(gb), cbuf, (uint32)n);
			return NkTensorInternal::MakeGpu(ga.Shape(), ga.DType(), cbuf);
		}

		static NkTensor GpuUnaryOp(const char *name, const char *src, const NkTensor &a) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			if (!ga.IsValid())
				return NkTensor{};
			const int64 n = ga.Numel();
			uint64 bbuf = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(ga.DType()));
			if (!bbuf)
				return NkTensor{};
			NkTensorGpu::Get().RunUnary(name, NkString(src), NkTensorInternal::GpuBuffer(ga), bbuf, (uint32)n);
			return NkTensorInternal::MakeGpu(ga.Shape(), ga.DType(), bbuf);
		}

		NkTensor NkGpuMul(const NkTensor &a, const NkTensor &b) {
			return GpuBinaryOp("mul", kMulNkSL, a, b);
		}

		NkTensor NkGpuSub(const NkTensor &a, const NkTensor &b) {
			return GpuBinaryOp("sub", kSubNkSL, a, b);
		}

		NkTensor NkGpuRelu(const NkTensor &a) {
			return GpuUnaryOp("relu", kReluNkSL, a);
		}

		NkTensor NkGpuSigmoid(const NkTensor &a) {
			return GpuUnaryOp("sigmoid", kSigmoidNkSL, a);
		}

		NkTensor NkGpuTanh(const NkTensor &a) {
			return GpuUnaryOp("tanh", kTanhNkSL, a);
		}

		// ---- Unaires à scalaire : mulscalar / addscalar / step (masque ReLU') -------
		static const char *kMulScalarNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; float s; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = A.data[i] * pc.s; } }
)NKSL";
		static const char *kAddScalarNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; float s; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = A.data[i] + pc.s; } }
)NKSL";
		static const char *kStepNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; float s; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = A.data[i] > 0.0 ? 1.0 : 0.0; } }
)NKSL";

		static NkTensor GpuUnaryScalarOp(const char *name, const char *src, const NkTensor &a, float s) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			if (!ga.IsValid())
				return NkTensor{};
			const int64 n = ga.Numel();
			uint64 bbuf = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(ga.DType()));
			if (!bbuf)
				return NkTensor{};
			NkTensorGpu::Get().RunUnaryScalar(name, NkString(src), NkTensorInternal::GpuBuffer(ga), bbuf, (uint32)n, s);
			return NkTensorInternal::MakeGpu(ga.Shape(), ga.DType(), bbuf);
		}

		NkTensor NkGpuMulScalar(const NkTensor &a, double s) {
			return GpuUnaryScalarOp("mulscalar", kMulScalarNkSL, a, (float)s);
		}

		NkTensor NkGpuAddScalar(const NkTensor &a, double s) {
			return GpuUnaryScalarOp("addscalar", kAddScalarNkSL, a, (float)s);
		}

		NkTensor NkGpuStep(const NkTensor &a) {
			return GpuUnaryScalarOp("step", kStepNkSL, a, 0.0f);
		}

		static const char *kDivNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { C.data[i] = A.data[i] / B.data[i]; } }
)NKSL";
		static const char *kSqrtNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = sqrt(A.data[i]); } }
)NKSL";

		NkTensor NkGpuDiv(const NkTensor &a, const NkTensor &b) {
			return GpuBinaryOp("div", kDivNkSL, a, b);
		}

		NkTensor NkGpuSqrt(const NkTensor &a) {
			return GpuUnaryOp("sqrt", kSqrtNkSL, a);
		}

		// ---- GELU (tanh-approx) : fwd unaire + bwd (recalcul depuis x) --------------
		static const char *kGeluNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        float x = A.data[i];
        float inner = 0.7978845608 * (x + 0.044715 * x*x*x);
        B.data[i] = 0.5 * x * (1.0 + tanh(inner));
    }
}
)NKSL";
		static const char *kGeluBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufG { float data[]; } G;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P { uint count; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < d.count) {
        float x = X.data[i]; float x2 = x*x; float c = 0.7978845608;
        float inner = c * (x + 0.044715 * x2 * x);
        float t = tanh(inner); float sech2 = 1.0 - t*t;
        float dg = 0.5*(1.0+t) + 0.5*x*sech2*c*(1.0 + 3.0*0.044715*x2);
        DX.data[i] = G.data[i] * dg;
    }
}
)NKSL";

		NkTensor NkGpuGelu(const NkTensor &a) {
			return GpuUnaryOp("gelu", kGeluNkSL, a);
		}

		NkTensor NkGpuGeluBackward(const NkTensor &x, const NkTensor &grad) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			if (!gx.IsValid() || !gg.IsValid())
				return NkTensor{};
			const int64 n = gx.Numel();
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(gx.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12] = {(uint32)n, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("gelu_bwd", NkString(kGeluBwdNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gg), db, p, (uint32)n);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), db);
		}

		// ---- Embedding : table[vocab,d], idx[..] (ids f32) -> [.., d] ; bwd scatter-add
		static const char *kEmbeddingFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufT { float data[]; } Tb;
@binding(set=0, binding=1) buffer BufI { float data[]; } Idx;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform P { uint numPos; uint d; uint vocab; } p;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint total = p.numPos * p.d;
    if (i < total) {
        uint pos = i / p.d; uint c = i % p.d;
        uint tid = uint(Idx.data[pos] + 0.5);
        O.data[i] = Tb.data[tid * p.d + c];
    }
}
)NKSL";
		static const char *kEmbeddingBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufI { float data[]; } Idx;
@binding(set=0, binding=2) buffer BufD { float data[]; } DT;
@binding(set=0, binding=3) uniform P { uint numPos; uint d; uint vocab; } p;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    uint total = p.vocab * p.d;
    if (e < total) {
        uint v = e / p.d; uint c = e % p.d;
        float acc = 0.0;
        for (uint pos = 0u; pos < p.numPos; pos = pos + 1u) {
            if (uint(Idx.data[pos] + 0.5) == v) acc = acc + G.data[pos * p.d + c];
        }
        DT.data[e] = acc;
    }
}
)NKSL";

		NkTensor NkGpuEmbedding(const NkTensor &table, const NkTensor &idx) {
			NkTensor gt = (table.Device() == NkDevice::NK_GPU) ? table : table.ToGPU();
			NkTensor gi = (idx.Device() == NkDevice::NK_GPU) ? idx : idx.ToGPU();
			if (!gt.IsValid() || !gi.IsValid() || gt.Rank() != 2)
				return NkTensor{};
			const int64 vocab = gt.Shape()[0], d = gt.Shape()[1];
			const int64 numPos = gi.Numel();
			// outShape = idx.Shape() + [d]
			NkShape outShape;
			outShape.Resize(gi.Rank() + 1);
			for (uint32 k = 0; k < gi.Rank(); ++k)
				outShape[k] = gi.Shape()[k];
			outShape[gi.Rank()] = d;
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)(numPos * d) * NkDTypeSize(gt.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)numPos, (uint32)d, (uint32)vocab, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("embedding_fwd", NkString(kEmbeddingFwdNkSL), NkTensorInternal::GpuBuffer(gt),
									  NkTensorInternal::GpuBuffer(gi), ob, p, (uint32)(numPos * d));
			return NkTensorInternal::MakeGpu(outShape, gt.DType(), ob);
		}

		NkTensor NkGpuEmbeddingBackward(const NkTensor &grad, const NkTensor &idx, int64 vocab, int64 d) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor gi = (idx.Device() == NkDevice::NK_GPU) ? idx : idx.ToGPU();
			if (!gg.IsValid() || !gi.IsValid())
				return NkTensor{};
			const int64 numPos = gi.Numel();
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)(vocab * d) * NkDTypeSize(gg.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12] = {(uint32)numPos, (uint32)d, (uint32)vocab, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("embedding_bwd", NkString(kEmbeddingBwdNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gi), db, p, (uint32)(vocab * d));
			return NkTensorInternal::MakeGpu(NkShape{vocab, d}, gg.DType(), db);
		}

		bool NkGpuAdamStep(const NkTensor &param, const NkTensor &grad, const NkTensor &m, const NkTensor &v, float lr,
						   float b1, float b2, float eps, float b1t, float b2t, float wd) {
			// Tous doivent résider sur GPU et être contigus de même taille.
			if (param.Device() != NkDevice::NK_GPU || grad.Device() != NkDevice::NK_GPU ||
				m.Device() != NkDevice::NK_GPU || v.Device() != NkDevice::NK_GPU)
				return false;
			const int64 n = param.Numel();
			if (grad.Numel() != n || m.Numel() != n || v.Numel() != n)
				return false;
			uint64 bp = NkTensorInternal::GpuBuffer(param), bg = NkTensorInternal::GpuBuffer(grad);
			uint64 bm = NkTensorInternal::GpuBuffer(m), bv = NkTensorInternal::GpuBuffer(v);
			if (!bp || !bg || !bm || !bv)
				return false;
			return NkTensorGpu::Get().RunAdam(bp, bg, bm, bv, (uint32)n, lr, b1, b2, eps, b1t, b2t, wd);
		}

		// ---- Réductions GPU : vue [outer, reduce, inner] -> [outer, inner] ----------
		static const char *kReduceSumNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Dims { uint outer; uint reduce; uint inner; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint total = d.outer * d.inner;
    if (idx < total) {
        uint o = idx / d.inner;
        uint i = idx - o * d.inner;
        uint base = o * d.reduce * d.inner + i;
        float acc = 0.0;
        for (uint r = 0u; r < d.reduce; r = r + 1u) { acc = acc + A.data[base + r * d.inner]; }
        B.data[idx] = acc;
    }
}
)NKSL";
		static const char *kReduceMeanNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Dims { uint outer; uint reduce; uint inner; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint total = d.outer * d.inner;
    if (idx < total) {
        uint o = idx / d.inner;
        uint i = idx - o * d.inner;
        uint base = o * d.reduce * d.inner + i;
        float acc = 0.0;
        for (uint r = 0u; r < d.reduce; r = r + 1u) { acc = acc + A.data[base + r * d.inner]; }
        B.data[idx] = acc / float(d.reduce);
    }
}
)NKSL";
		static const char *kReduceMaxNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Dims { uint outer; uint reduce; uint inner; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint total = d.outer * d.inner;
    if (idx < total) {
        uint o = idx / d.inner;
        uint i = idx - o * d.inner;
        uint base = o * d.reduce * d.inner + i;
        float acc = A.data[base];
        for (uint r = 1u; r < d.reduce; r = r + 1u) { float v = A.data[base + r * d.inner]; acc = v > acc ? v : acc; }
        B.data[idx] = acc;
    }
}
)NKSL";

		static const char *kReduceArgmaxNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Dims { uint outer; uint reduce; uint inner; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint total = d.outer * d.inner;
    if (idx < total) {
        uint o = idx / d.inner;
        uint i = idx - o * d.inner;
        uint base = o * d.reduce * d.inner + i;
        float best = A.data[base]; uint bi = 0u;
        for (uint r = 1u; r < d.reduce; r = r + 1u) {
            float v = A.data[base + r * d.inner];
            if (v > best) { best = v; bi = r; }
        }
        B.data[idx] = float(bi);   // indice de l'argmax (stocké en f32)
    }
}
)NKSL";

		static NkTensor GpuReduceImpl(const NkTensor &a, bool hasAxis, uint32 axis, int kind) {
			NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
			if (!ga.IsValid())
				return NkTensor{};
			const uint32 r = ga.Rank();
			uint32 outer = 1, reduce = 1, inner = 1;
			NkShape outShape;
			if (!hasAxis || r <= 1) {
				// Réduction globale -> scalaire {1}.
				reduce = (uint32)ga.Numel();
				outShape.PushBack(1);
			} else {
				for (uint32 i = 0; i < axis; i++)
					outer *= (uint32)ga.Shape()[i];
				reduce = (uint32)ga.Shape()[axis];
				for (uint32 i = axis + 1; i < r; i++)
					inner *= (uint32)ga.Shape()[i];
				outShape.Resize(r - 1);
				for (uint32 i = 0, oi = 0; i < r; i++)
					if (i != axis)
						outShape[oi++] = ga.Shape()[i];
			}
			const uint32 outN = outer * inner;
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)outN * NkDTypeSize(ga.DType()));
			if (!obuf)
				return NkTensor{};
			const char *name;
			const char *src;
			switch (kind) {
				case 1:
					name = "reduce_mean";
					src = kReduceMeanNkSL;
					break;
				case 2:
					name = "reduce_max";
					src = kReduceMaxNkSL;
					break;
				case 3:
					name = "reduce_argmax";
					src = kReduceArgmaxNkSL;
					break;
				default:
					name = "reduce_sum";
					src = kReduceSumNkSL;
					break;
			}
			NkTensorGpu::Get().RunReduce(name, NkString(src), NkTensorInternal::GpuBuffer(ga), obuf, outer, reduce,
										 inner);
			return NkTensorInternal::MakeGpu(outShape, ga.DType(), obuf);
		}

		NkTensor NkGpuReduceAll(const NkTensor &a, int kind) {
			return GpuReduceImpl(a, false, 0, kind);
		}

		NkTensor NkGpuReduceAxis(const NkTensor &a, uint32 axis, int kind) {
			return GpuReduceImpl(a, true, axis, kind);
		}

		// Matérialise une vue GPU strided (permute/transpose) en buffer contigu, sur GPU.
		NkTensor NkGpuContiguous(const NkTensor &t) {
			const uint32 r = t.Rank();
			if (t.Device() != NkDevice::NK_GPU)
				return t.Contiguous();
			if (r > 8)
				return t.ToCPU().Contiguous().ToGPU(); // repli (rang non supporté)
			uint32 shape[8], strides[8];
			for (uint32 i = 0; i < r; i++) {
				shape[i] = (uint32)t.Shape()[i];
				strides[i] = (uint32)t.Strides()[i];
			}
			const uint32 count = (uint32)t.Numel();
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)count * NkDTypeSize(t.DType()));
			if (!obuf)
				return NkTensor{};
			NkTensorGpu::Get().RunGather(NkTensorInternal::GpuBuffer(t), obuf, r, (uint32)NkTensorInternal::Offset(t),
										 shape, strides, count);
			return NkTensorInternal::MakeGpu(t.Shape(), t.DType(), obuf); // strides contigus
		}

		// ---- im2col / col2im GPU (conv comme réarrangement mémoire) -----------------
		// UBO : {B,Cin,H,W,kH,kW,stride,pad,outH,outW,K,M} (indices 0..11).
		static const char *kIm2ColNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P {
    uint B_; uint Cin; uint H; uint W; uint kH; uint kW;
    uint stride; uint pad; uint outH; uint outW; uint K; uint M;
} p;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    if (e < p.M * p.K) {
        uint row  = e / p.K;
        uint kcol = e - row * p.K;
        uint ow = row % p.outW; uint t = row / p.outW;
        uint oh = t % p.outH;   uint b = t / p.outH;
        uint kx = kcol % p.kW;  uint t2 = kcol / p.kW;
        uint ky = t2 % p.kH;    uint ic = t2 / p.kH;
        int iy = int(oh * p.stride) - int(p.pad) + int(ky);
        int ix = int(ow * p.stride) - int(p.pad) + int(kx);
        float v = 0.0;
        if (iy >= 0 && iy < int(p.H) && ix >= 0 && ix < int(p.W)) {
            uint xi = ((b * p.Cin + ic) * p.H + uint(iy)) * p.W + uint(ix);
            v = A.data[xi];
        }
        B.data[e] = v;
    }
}
)NKSL";
		// col2im par GATHER : un thread par élément de dx[b,ic,iy,ix], somme les
		// colonnes (oh,ow,ky,kx) qui retombent dessus. Pas d'atomics -> pas de course.
		static const char *kCol2ImNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P {
    uint B_; uint Cin; uint H; uint W; uint kH; uint kW;
    uint stride; uint pad; uint outH; uint outW; uint K; uint M;
} p;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    uint total = p.B_ * p.Cin * p.H * p.W;
    if (e < total) {
        uint ix = e % p.W; uint t = e / p.W;
        uint iy = t % p.H; t = t / p.H;
        uint ic = t % p.Cin; uint b = t / p.Cin;
        float acc = 0.0;
        for (uint ky = 0u; ky < p.kH; ky = ky + 1u) {
            int ohn = int(iy) + int(p.pad) - int(ky);
            bool okY = (ohn >= 0) && (uint(ohn) % p.stride == 0u);
            uint oh = okY ? (uint(ohn) / p.stride) : 0u;
            okY = okY && (oh < p.outH);
            if (okY) {
                for (uint kx = 0u; kx < p.kW; kx = kx + 1u) {
                    int own = int(ix) + int(p.pad) - int(kx);
                    bool okX = (own >= 0) && (uint(own) % p.stride == 0u);
                    uint ow = okX ? (uint(own) / p.stride) : 0u;
                    okX = okX && (ow < p.outW);
                    if (okX) {
                        uint row  = (b * p.outH + oh) * p.outW + ow;
                        uint kcol = (ic * p.kH + ky) * p.kW + kx;
                        acc = acc + A.data[row * p.K + kcol];
                    }
                }
            }
        }
        B.data[e] = acc;
    }
}
)NKSL";

		NkTensor NkGpuIm2Col(const NkTensor &x, int64 kH, int64 kW, int64 stride, int64 pad, int64 outH, int64 outW) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() != 4)
				return NkTensor{};
			const int64 B = gx.Shape()[0], Cin = gx.Shape()[1], H = gx.Shape()[2], W = gx.Shape()[3];
			const int64 K = Cin * kH * kW, M = B * outH * outW;
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)(M * K) * NkDTypeSize(gx.DType()));
			if (!obuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)B,		(uint32)Cin, (uint32)H,	   (uint32)W,	 (uint32)kH, (uint32)kW,
							(uint32)stride, (uint32)pad, (uint32)outH, (uint32)outW, (uint32)K,	 (uint32)M};
			NkTensorGpu::Get().RunConvOp("im2col", NkString(kIm2ColNkSL), NkTensorInternal::GpuBuffer(gx), obuf, p,
										 (uint32)(M * K));
			return NkTensorInternal::MakeGpu(NkShape{M, K}, gx.DType(), obuf);
		}

		NkTensor NkGpuCol2Im(const NkTensor &col, int64 B, int64 Cin, int64 H, int64 W, int64 kH, int64 kW,
							 int64 stride, int64 pad, int64 outH, int64 outW) {
			NkTensor gc = (col.Device() == NkDevice::NK_GPU) ? col : col.ToGPU();
			if (!gc.IsValid())
				return NkTensor{};
			const int64 K = Cin * kH * kW, M = B * outH * outW, total = B * Cin * H * W;
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)total * NkDTypeSize(gc.DType()));
			if (!obuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)B,		(uint32)Cin, (uint32)H,	   (uint32)W,	 (uint32)kH, (uint32)kW,
							(uint32)stride, (uint32)pad, (uint32)outH, (uint32)outW, (uint32)K,	 (uint32)M};
			NkTensorGpu::Get().RunConvOp("col2im", NkString(kCol2ImNkSL), NkTensorInternal::GpuBuffer(gc), obuf, p,
										 (uint32)total);
			return NkTensorInternal::MakeGpu(NkShape{B, Cin, H, W}, gc.DType(), obuf);
		}

		// ---- Softmax par ligne GPU (stable) : [rows, cols] -------------------------
		static const char *kSoftmaxRowsNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P { uint rows; uint cols; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.cols;
        float mx = A.data[base];
        for (uint c = 1u; c < d.cols; c = c + 1u) { float v = A.data[base + c]; if (v > mx) mx = v; }
        float sum = 0.0;
        for (uint c = 0u; c < d.cols; c = c + 1u) { float e = exp(A.data[base + c] - mx); B.data[base + c] = e; sum = sum + e; }
        float inv = sum > 0.0 ? 1.0 / sum : 0.0;
        for (uint c = 0u; c < d.cols; c = c + 1u) { B.data[base + c] = B.data[base + c] * inv; }
    }
}
)NKSL";
		// ---- LayerNorm (dernier axe) GPU : fwd (x->y) + bwd (x,g->dx), ε=1e-5 ------
		static const char *kLayerNormFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P { uint rows; uint D; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.D; float fD = float(d.D);
        float mean = 0.0; for (uint c=0u;c<d.D;c=c+1u) mean = mean + A.data[base+c]; mean = mean / fD;
        float var = 0.0; for (uint c=0u;c<d.D;c=c+1u) { float t = A.data[base+c]-mean; var = var + t*t; } var = var / fD;
        float invstd = 1.0 / sqrt(var + 1e-5);
        for (uint c=0u;c<d.D;c=c+1u) B.data[base+c] = (A.data[base+c]-mean) * invstd;
    }
}
)NKSL";
		static const char *kLayerNormBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufG { float data[]; } G;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P { uint rows; uint D; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.D; float fD = float(d.D);
        float mean = 0.0; for (uint c=0u;c<d.D;c=c+1u) mean = mean + X.data[base+c]; mean = mean / fD;
        float var = 0.0; for (uint c=0u;c<d.D;c=c+1u) { float t = X.data[base+c]-mean; var = var + t*t; } var = var / fD;
        float invstd = 1.0 / sqrt(var + 1e-5);
        float m1 = 0.0; float m2 = 0.0;
        for (uint c=0u;c<d.D;c=c+1u) { float xhat = (X.data[base+c]-mean)*invstd; m1 = m1 + G.data[base+c]; m2 = m2 + G.data[base+c]*xhat; }
        m1 = m1 / fD; m2 = m2 / fD;
        for (uint c=0u;c<d.D;c=c+1u) { float xhat = (X.data[base+c]-mean)*invstd; DX.data[base+c] = invstd*(G.data[base+c] - m1 - xhat*m2); }
    }
}
)NKSL";

		NkTensor NkGpuLayerNormStd(const NkTensor &x) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() < 1)
				return NkTensor{};
			const int64 D = gx.Shape()[gx.Rank() - 1];
			const int64 rows = (D > 0) ? gx.Numel() / D : 0;
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)gx.Numel() * NkDTypeSize(gx.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)D, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunConvOp("layernorm_fwd", NkString(kLayerNormFwdNkSL), NkTensorInternal::GpuBuffer(gx),
										 ob, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), ob);
		}

		NkTensor NkGpuLayerNormStdBackward(const NkTensor &x, const NkTensor &grad) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			if (!gx.IsValid() || !gg.IsValid() || gx.Rank() < 1)
				return NkTensor{};
			const int64 D = gx.Shape()[gx.Rank() - 1];
			const int64 rows = (D > 0) ? gx.Numel() / D : 0;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)gx.Numel() * NkDTypeSize(gx.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)D, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("layernorm_bwd", NkString(kLayerNormBwdNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gg), db, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), db);
		}

		NkTensor NkGpuSoftmaxRows(const NkTensor &x) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() < 1)
				return NkTensor{};
			const int64 n = gx.Numel();
			const int64 cols = gx.Shape()[gx.Rank() - 1]; // softmax sur le DERNIER axe
			const int64 rows = (cols > 0) ? n / cols : 0;
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(gx.DType()));
			if (!obuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)cols, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunConvOp("softmax_rows", NkString(kSoftmaxRowsNkSL), NkTensorInternal::GpuBuffer(gx),
										 obuf, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), obuf);
		}

		// Softmax backward : dx = y ⊙ (dy − Σ_lastaxis(dy⊙y)). y = sortie du softmax.
		static const char *kSoftmaxBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufY { float data[]; } Y;
@binding(set=0, binding=1) buffer BufG { float data[]; } G;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P { uint rows; uint cols; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.cols;
        float s = 0.0;
        for (uint c=0u;c<d.cols;c=c+1u) s = s + G.data[base+c]*Y.data[base+c];
        for (uint c=0u;c<d.cols;c=c+1u) DX.data[base+c] = Y.data[base+c]*(G.data[base+c] - s);
    }
}
)NKSL";

		NkTensor NkGpuSoftmaxBackward(const NkTensor &y, const NkTensor &grad) {
			NkTensor gy = (y.Device() == NkDevice::NK_GPU) ? y : y.ToGPU();
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			if (!gy.IsValid() || !gg.IsValid() || gy.Rank() < 1)
				return NkTensor{};
			const int64 cols = gy.Shape()[gy.Rank() - 1];
			const int64 rows = (cols > 0) ? gy.Numel() / cols : 0;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)gy.Numel() * NkDTypeSize(gy.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)cols, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("softmax_bwd", NkString(kSoftmaxBwdNkSL), NkTensorInternal::GpuBuffer(gy),
									  NkTensorInternal::GpuBuffer(gg), db, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gy.Shape(), gy.DType(), db);
		}

		// Softmax CAUSAL : dernier axe [.., T, T], position requête = row % T ; masque j>pos.
		static const char *kSoftmaxCausalNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P { uint rows; uint T; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.T;
        uint pos = r % d.T;                       // indice de la requête
        float mx = A.data[base];
        for (uint c=1u;c<=pos;c=c+1u) { float v=A.data[base+c]; if (v>mx) mx=v; }
        float sum = 0.0;
        for (uint c=0u;c<=pos;c=c+1u) { float e=exp(A.data[base+c]-mx); B.data[base+c]=e; sum=sum+e; }
        float inv = sum>0.0 ? 1.0/sum : 0.0;
        for (uint c=0u;c<=pos;c=c+1u) B.data[base+c]=B.data[base+c]*inv;
        for (uint c=pos+1u;c<d.T;c=c+1u) B.data[base+c]=0.0;   // futur masqué
    }
}
)NKSL";

		NkTensor NkGpuSoftmaxCausal(const NkTensor &x) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() < 2)
				return NkTensor{};
			const int64 T = gx.Shape()[gx.Rank() - 1];
			const int64 rows = (T > 0) ? gx.Numel() / T : 0;
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)gx.Numel() * NkDTypeSize(gx.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)T, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunConvOp("softmax_causal", NkString(kSoftmaxCausalNkSL),
										 NkTensorInternal::GpuBuffer(gx), ob, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), ob);
		}

		// ---- Max-pooling 2D GPU ----------------------------------------------------
		// UBO (12 uints) : {B,C,H,W,oH,oW,kernel,stride, ...}.
		static const char *kMaxPoolFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufI { float data[]; } I;
@binding(set=0, binding=1) buffer BufO { float data[]; } O;
@binding(set=0, binding=2) buffer BufA { float data[]; } Arg;
@binding(set=0, binding=3) uniform P {
    uint B; uint C; uint H; uint W; uint oH; uint oW; uint ker; uint stride;
} d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint total = d.B * d.C * d.oH * d.oW;
    if (i < total) {
        uint ox = i % d.oW; uint t = i / d.oW;
        uint oy = t % d.oH; t = t / d.oH;
        uint c = t % d.C;   uint b = t / d.C;
        float best = -1.0e30; uint bidx = 0u;
        for (uint ky = 0u; ky < d.ker; ky = ky + 1u)
        for (uint kx = 0u; kx < d.ker; kx = kx + 1u) {
            uint iy = oy * d.stride + ky; uint ix = ox * d.stride + kx;
            float v = I.data[((b * d.C + c) * d.H + iy) * d.W + ix];
            if (v > best) { best = v; bidx = iy * d.W + ix; }
        }
        O.data[i] = best; Arg.data[i] = float(bidx);
    }
}
)NKSL";
		static const char *kMaxPoolBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufA { float data[]; } Arg;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P {
    uint B; uint C; uint H; uint W; uint oH; uint oW; uint ker; uint stride;
} d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    uint total = d.B * d.C * d.H * d.W;
    if (e < total) {
        uint ix = e % d.W; uint t = e / d.W;
        uint iy = t % d.H; t = t / d.H;
        uint c = t % d.C;  uint b = t / d.C;
        uint oyStart = (iy >= d.ker) ? ((iy - d.ker) / d.stride + 1u) : 0u;
        uint oyEnd   = iy / d.stride;
        uint oxStart = (ix >= d.ker) ? ((ix - d.ker) / d.stride + 1u) : 0u;
        uint oxEnd   = ix / d.stride;
        float acc = 0.0;
        for (uint oy = oyStart; oy <= oyEnd && oy < d.oH; oy = oy + 1u)
        for (uint ox = oxStart; ox <= oxEnd && ox < d.oW; ox = ox + 1u) {
            uint oidx = ((b * d.C + c) * d.oH + oy) * d.oW + ox;
            uint arg = uint(Arg.data[oidx] + 0.5);
            if (arg == iy * d.W + ix) acc = acc + G.data[oidx];
        }
        DX.data[e] = acc;
    }
}
)NKSL";

		NkTensor NkGpuMaxPool2D(const NkTensor &x, int64 kernel, int64 stride, NkTensor &argOut) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() != 4)
				return NkTensor{};
			const int64 B = gx.Shape()[0], C = gx.Shape()[1], H = gx.Shape()[2], W = gx.Shape()[3];
			const int64 oH = (H - kernel) / stride + 1, oW = (W - kernel) / stride + 1;
			const int64 no = B * C * oH * oW;
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(gx.DType()));
			uint64 abuf = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(gx.DType()));
			if (!obuf || !abuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)B,		(uint32)C,		(uint32)H, (uint32)W, (uint32)oH, (uint32)oW,
							(uint32)kernel, (uint32)stride, 0,		   0,		  0,		  0};
			NkTensorGpu::Get().RunOp3("maxpool_fwd", NkString(kMaxPoolFwdNkSL), NkTensorInternal::GpuBuffer(gx), obuf,
									  abuf, p, (uint32)no);
			argOut = NkTensorInternal::MakeGpu(NkShape{B, C, oH, oW}, gx.DType(), abuf);
			return NkTensorInternal::MakeGpu(NkShape{B, C, oH, oW}, gx.DType(), obuf);
		}

		NkTensor NkGpuMaxPool2DBackward(const NkTensor &grad, const NkTensor &arg, int64 B, int64 C, int64 H, int64 W,
										int64 outH, int64 outW, int64 kernel, int64 stride) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor ga = (arg.Device() == NkDevice::NK_GPU) ? arg : arg.ToGPU();
			if (!gg.IsValid() || !ga.IsValid())
				return NkTensor{};
			const int64 ni = B * C * H * W;
			uint64 dbuf = NkTensorGpu::Get().CreateBuffer((nk_size)ni * NkDTypeSize(gg.DType()));
			if (!dbuf)
				return NkTensor{};
			uint32 p[12] = {
				(uint32)B, (uint32)C, (uint32)H, (uint32)W, (uint32)outH, (uint32)outW, (uint32)kernel, (uint32)stride,
				0,		   0,		  0,		 0};
			NkTensorGpu::Get().RunOp3("maxpool_bwd", NkString(kMaxPoolBwdNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(ga), dbuf, p, (uint32)ni);
			return NkTensorInternal::MakeGpu(NkShape{B, C, H, W}, gg.DType(), dbuf);
		}

		// ---- Exp élémentaire GPU ---------------------------------------------------
		static const char *kExpNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() { uint i = gl_GlobalInvocationID.x; if (i < pc.count) { B.data[i] = exp(A.data[i]); } }
)NKSL";

		NkTensor NkGpuExp(const NkTensor &a) {
			return GpuUnaryOp("exp", kExpNkSL, a);
		}

		// ---- Upsample nearest ×2 GPU (forward + backward) --------------------------
		static const char *kUpsampleFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P { uint B_; uint C; uint H; uint W; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint oH = 2u * d.H; uint oW = 2u * d.W;
    uint total = d.B_ * d.C * oH * oW;
    if (i < total) {
        uint ox = i % oW; uint t = i / oW;
        uint oy = t % oH; t = t / oH;
        uint c = t % d.C; uint b = t / d.C;
        uint iy = oy / 2u; uint ix = ox / 2u;
        B.data[i] = A.data[((b * d.C + c) * d.H + iy) * d.W + ix];
    }
}
)NKSL";
		static const char *kUpsampleBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufD { float data[]; } D;
@binding(set=0, binding=2) uniform P { uint B_; uint C; uint H; uint W; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    uint total = d.B_ * d.C * d.H * d.W;
    if (e < total) {
        uint ix = e % d.W; uint t = e / d.W;
        uint iy = t % d.H; t = t / d.H;
        uint c = t % d.C; uint b = t / d.C;
        uint oH = 2u * d.H; uint oW = 2u * d.W;
        float s = 0.0;
        for (uint dy = 0u; dy < 2u; dy = dy + 1u)
        for (uint dx = 0u; dx < 2u; dx = dx + 1u)
            s = s + G.data[((b * d.C + c) * oH + (2u * iy + dy)) * oW + (2u * ix + dx)];
        D.data[e] = s;
    }
}
)NKSL";

		NkTensor NkGpuUpsample2x(const NkTensor &x) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() != 4)
				return NkTensor{};
			const int64 B = gx.Shape()[0], C = gx.Shape()[1], H = gx.Shape()[2], W = gx.Shape()[3];
			const int64 no = B * C * (2 * H) * (2 * W);
			uint64 obuf = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(gx.DType()));
			if (!obuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)B, (uint32)C, (uint32)H, (uint32)W, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunConvOp("upsample_fwd", NkString(kUpsampleFwdNkSL), NkTensorInternal::GpuBuffer(gx),
										 obuf, p, (uint32)no);
			return NkTensorInternal::MakeGpu(NkShape{B, C, 2 * H, 2 * W}, gx.DType(), obuf);
		}

		NkTensor NkGpuUpsample2xBackward(const NkTensor &grad, int64 B, int64 C, int64 H, int64 W) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			if (!gg.IsValid())
				return NkTensor{};
			const int64 ni = B * C * H * W;
			uint64 dbuf = NkTensorGpu::Get().CreateBuffer((nk_size)ni * NkDTypeSize(gg.DType()));
			if (!dbuf)
				return NkTensor{};
			uint32 p[12] = {(uint32)B, (uint32)C, (uint32)H, (uint32)W, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunConvOp("upsample_bwd", NkString(kUpsampleBwdNkSL), NkTensorInternal::GpuBuffer(gg),
										 dbuf, p, (uint32)ni);
			return NkTensorInternal::MakeGpu(NkShape{B, C, H, W}, gg.DType(), dbuf);
		}

		// ---- ConvTranspose2D GPU (fwd + dX + dW), formulations gather (sans course) --
		// UBO (12 uints) : {B,Cin,H,W,Cout,kH,kW,stride,pad,outH,outW, _}.
		static const char *kConvT2dFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wt;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform P {
    uint B; uint Cin; uint H; uint W; uint Cout; uint kH; uint kW; uint stride; uint pad; uint oH; uint oW;
} d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint total = d.B * d.Cout * d.oH * d.oW;
    if (i < total) {
        uint ox = i % d.oW; uint t = i / d.oW;
        uint oy = t % d.oH; t = t / d.oH;
        uint oc = t % d.Cout; uint b = t / d.Cout;
        float acc = 0.0;
        for (uint ky = 0u; ky < d.kH; ky = ky + 1u) {
            int iyn = int(oy) + int(d.pad) - int(ky);
            bool okY = (iyn >= 0) && (uint(iyn) % d.stride == 0u);
            uint iy = okY ? (uint(iyn) / d.stride) : 0u; okY = okY && (iy < d.H);
            if (okY) {
                for (uint kx = 0u; kx < d.kW; kx = kx + 1u) {
                    int ixn = int(ox) + int(d.pad) - int(kx);
                    bool okX = (ixn >= 0) && (uint(ixn) % d.stride == 0u);
                    uint ix = okX ? (uint(ixn) / d.stride) : 0u; okX = okX && (ix < d.W);
                    if (okX) {
                        for (uint ic = 0u; ic < d.Cin; ic = ic + 1u) {
                            acc = acc + X.data[((b * d.Cin + ic) * d.H + iy) * d.W + ix]
                                      * Wt.data[((ic * d.Cout + oc) * d.kH + ky) * d.kW + kx];
                        }
                    }
                }
            }
        }
        Y.data[i] = acc;
    }
}
)NKSL";
		static const char *kConvT2dDxNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wt;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P {
    uint B; uint Cin; uint H; uint W; uint Cout; uint kH; uint kW; uint stride; uint pad; uint oH; uint oW;
} d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    uint total = d.B * d.Cin * d.H * d.W;
    if (e < total) {
        uint ix = e % d.W; uint t = e / d.W;
        uint iy = t % d.H; t = t / d.H;
        uint ic = t % d.Cin; uint b = t / d.Cin;
        float acc = 0.0;
        for (uint oc = 0u; oc < d.Cout; oc = oc + 1u)
        for (uint ky = 0u; ky < d.kH; ky = ky + 1u)
        for (uint kx = 0u; kx < d.kW; kx = kx + 1u) {
            int oy = int(iy) * int(d.stride) - int(d.pad) + int(ky);
            int ox = int(ix) * int(d.stride) - int(d.pad) + int(kx);
            if (oy >= 0 && oy < int(d.oH) && ox >= 0 && ox < int(d.oW)) {
                acc = acc + G.data[((b * d.Cout + oc) * d.oH + uint(oy)) * d.oW + uint(ox)]
                          * Wt.data[((ic * d.Cout + oc) * d.kH + ky) * d.kW + kx];
            }
        }
        DX.data[e] = acc;
    }
}
)NKSL";
		static const char *kConvT2dDwNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufG { float data[]; } G;
@binding(set=0, binding=2) buffer BufD { float data[]; } DW;
@binding(set=0, binding=3) uniform P {
    uint B; uint Cin; uint H; uint W; uint Cout; uint kH; uint kW; uint stride; uint pad; uint oH; uint oW;
} d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint e = gl_GlobalInvocationID.x;
    uint total = d.Cin * d.Cout * d.kH * d.kW;
    if (e < total) {
        uint kx = e % d.kW; uint t = e / d.kW;
        uint ky = t % d.kH; t = t / d.kH;
        uint oc = t % d.Cout; uint ic = t / d.Cout;
        float acc = 0.0;
        for (uint b = 0u; b < d.B; b = b + 1u)
        for (uint iy = 0u; iy < d.H; iy = iy + 1u)
        for (uint ix = 0u; ix < d.W; ix = ix + 1u) {
            int oy = int(iy) * int(d.stride) - int(d.pad) + int(ky);
            int ox = int(ix) * int(d.stride) - int(d.pad) + int(kx);
            if (oy >= 0 && oy < int(d.oH) && ox >= 0 && ox < int(d.oW)) {
                acc = acc + X.data[((b * d.Cin + ic) * d.H + iy) * d.W + ix]
                          * G.data[((b * d.Cout + oc) * d.oH + uint(oy)) * d.oW + uint(ox)];
            }
        }
        DW.data[e] = acc;
    }
}
)NKSL";

		static void ConvT2dParams(uint32 *p, int64 B, int64 Cin, int64 H, int64 W, int64 Cout, int64 kH, int64 kW,
								  int64 stride, int64 pad, int64 oH, int64 oW) {
			p[0] = (uint32)B;
			p[1] = (uint32)Cin;
			p[2] = (uint32)H;
			p[3] = (uint32)W;
			p[4] = (uint32)Cout;
			p[5] = (uint32)kH;
			p[6] = (uint32)kW;
			p[7] = (uint32)stride;
			p[8] = (uint32)pad;
			p[9] = (uint32)oH;
			p[10] = (uint32)oW;
			p[11] = 0u;
		}

		NkTensor NkGpuConvTranspose2D(const NkTensor &x, const NkTensor &w, int64 stride, int64 pad) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gw = (w.Device() == NkDevice::NK_GPU) ? w : w.ToGPU();
			if (!gx.IsValid() || !gw.IsValid() || gx.Rank() != 4 || gw.Rank() != 4)
				return NkTensor{};
			const int64 B = gx.Shape()[0], Cin = gx.Shape()[1], H = gx.Shape()[2], W = gx.Shape()[3];
			const int64 Cout = gw.Shape()[1], kH = gw.Shape()[2], kW = gw.Shape()[3];
			const int64 oH = (H - 1) * stride - 2 * pad + kH, oW = (W - 1) * stride - 2 * pad + kW;
			const int64 no = B * Cout * oH * oW;
			uint64 ybuf = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(gx.DType()));
			if (!ybuf)
				return NkTensor{};
			uint32 p[12];
			ConvT2dParams(p, B, Cin, H, W, Cout, kH, kW, stride, pad, oH, oW);
			NkTensorGpu::Get().RunOp3("convt2d_fwd", NkString(kConvT2dFwdNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gw), ybuf, p, (uint32)no);
			return NkTensorInternal::MakeGpu(NkShape{B, Cout, oH, oW}, gx.DType(), ybuf);
		}

		NkTensor NkGpuConvTranspose2DBackwardX(const NkTensor &grad, const NkTensor &w, int64 B, int64 Cin, int64 H,
											   int64 W, int64 Cout, int64 kH, int64 kW, int64 stride, int64 pad,
											   int64 outH, int64 outW) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor gw = (w.Device() == NkDevice::NK_GPU) ? w : w.ToGPU();
			if (!gg.IsValid() || !gw.IsValid())
				return NkTensor{};
			const int64 ni = B * Cin * H * W;
			uint64 dbuf = NkTensorGpu::Get().CreateBuffer((nk_size)ni * NkDTypeSize(gg.DType()));
			if (!dbuf)
				return NkTensor{};
			uint32 p[12];
			ConvT2dParams(p, B, Cin, H, W, Cout, kH, kW, stride, pad, outH, outW);
			NkTensorGpu::Get().RunOp3("convt2d_dx", NkString(kConvT2dDxNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gw), dbuf, p, (uint32)ni);
			return NkTensorInternal::MakeGpu(NkShape{B, Cin, H, W}, gg.DType(), dbuf);
		}

		NkTensor NkGpuConvTranspose2DBackwardW(const NkTensor &x, const NkTensor &grad, int64 B, int64 Cin, int64 H,
											   int64 W, int64 Cout, int64 kH, int64 kW, int64 stride, int64 pad,
											   int64 outH, int64 outW) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			if (!gx.IsValid() || !gg.IsValid())
				return NkTensor{};
			const int64 nw = Cin * Cout * kH * kW;
			uint64 dwbuf = NkTensorGpu::Get().CreateBuffer((nk_size)nw * NkDTypeSize(gx.DType()));
			if (!dwbuf)
				return NkTensor{};
			uint32 p[12];
			ConvT2dParams(p, B, Cin, H, W, Cout, kH, kW, stride, pad, outH, outW);
			NkTensorGpu::Get().RunOp3("convt2d_dw", NkString(kConvT2dDwNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gg), dwbuf, p, (uint32)nw);
			return NkTensorInternal::MakeGpu(NkShape{Cin, Cout, kH, kW}, gx.DType(), dwbuf);
		}

		// ===== Conv3D / ConvTranspose3D GPU (voxels) — UBO {B,Cin,D,H,W,Cout,kD,kH,kW,stride,pad} =====
		// oD/oH/oW calculés DANS le kernel (conv : (D+2p-k)/s+1 ; convT : (D-1)*s-2p+k).
		static void Conv3dParams(uint32 *p, int64 B, int64 Cin, int64 D, int64 H, int64 W, int64 Cout, int64 kD,
								 int64 kH, int64 kW, int64 stride, int64 pad) {
			p[0] = (uint32)B;
			p[1] = (uint32)Cin;
			p[2] = (uint32)D;
			p[3] = (uint32)H;
			p[4] = (uint32)W;
			p[5] = (uint32)Cout;
			p[6] = (uint32)kD;
			p[7] = (uint32)kH;
			p[8] = (uint32)kW;
			p[9] = (uint32)stride;
			p[10] = (uint32)pad;
			p[11] = 0u;
		}

		// ---- Conv3D forward : gather par sortie [B,Cout,oD,oH,oW] -------------------
		static const char *kConv3dFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wt;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform P { uint B; uint Cin; uint D; uint H; uint W; uint Cout; uint kD; uint kH; uint kW; uint stride; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint oD = (d.D + 2u*d.pad - d.kD)/d.stride + 1u;
    uint oH = (d.H + 2u*d.pad - d.kH)/d.stride + 1u;
    uint oW = (d.W + 2u*d.pad - d.kW)/d.stride + 1u;
    uint i = gl_GlobalInvocationID.x;
    uint total = d.B * d.Cout * oD * oH * oW;
    if (i < total) {
        uint ox = i % oW; uint t = i / oW; uint oy = t % oH; t = t / oH;
        uint od = t % oD; t = t / oD; uint oc = t % d.Cout; uint b = t / d.Cout;
        float acc = 0.0;
        for (uint ic = 0u; ic < d.Cin; ic = ic + 1u)
        for (uint kz = 0u; kz < d.kD; kz = kz + 1u)
        for (uint ky = 0u; ky < d.kH; ky = ky + 1u)
        for (uint kx = 0u; kx < d.kW; kx = kx + 1u) {
            int iz = int(od)*int(d.stride) - int(d.pad) + int(kz);
            int iy = int(oy)*int(d.stride) - int(d.pad) + int(ky);
            int ix = int(ox)*int(d.stride) - int(d.pad) + int(kx);
            if (iz>=0 && iz<int(d.D) && iy>=0 && iy<int(d.H) && ix>=0 && ix<int(d.W)) {
                acc = acc + X.data[((((b*d.Cin+ic)*d.D+uint(iz))*d.H+uint(iy))*d.W+uint(ix))]
                          * Wt.data[((((oc*d.Cin+ic)*d.kD+kz)*d.kH+ky)*d.kW+kx)];
            }
        }
        Y.data[i] = acc;
    }
}
)NKSL";
		// ---- Conv3D dX : gather par entrée [B,Cin,D,H,W] ---------------------------
		static const char *kConv3dDxNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wt;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P { uint B; uint Cin; uint D; uint H; uint W; uint Cout; uint kD; uint kH; uint kW; uint stride; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint oD = (d.D + 2u*d.pad - d.kD)/d.stride + 1u;
    uint oH = (d.H + 2u*d.pad - d.kH)/d.stride + 1u;
    uint oW = (d.W + 2u*d.pad - d.kW)/d.stride + 1u;
    uint e = gl_GlobalInvocationID.x;
    uint total = d.B * d.Cin * d.D * d.H * d.W;
    if (e < total) {
        uint ix = e % d.W; uint t = e / d.W; uint iy = t % d.H; t = t / d.H;
        uint iz = t % d.D; t = t / d.D; uint ic = t % d.Cin; uint b = t / d.Cin;
        float acc = 0.0;
        for (uint oc = 0u; oc < d.Cout; oc = oc + 1u)
        for (uint kz = 0u; kz < d.kD; kz = kz + 1u)
        for (uint ky = 0u; ky < d.kH; ky = ky + 1u)
        for (uint kx = 0u; kx < d.kW; kx = kx + 1u) {
            int odn = int(iz) + int(d.pad) - int(kz);
            int oyn = int(iy) + int(d.pad) - int(ky);
            int oxn = int(ix) + int(d.pad) - int(kx);
            bool ok = (odn>=0)&&(uint(odn)%d.stride==0u) && (oyn>=0)&&(uint(oyn)%d.stride==0u) && (oxn>=0)&&(uint(oxn)%d.stride==0u);
            if (ok) {
                uint od = uint(odn)/d.stride; uint oy = uint(oyn)/d.stride; uint ox = uint(oxn)/d.stride;
                if (od<oD && oy<oH && ox<oW) {
                    acc = acc + G.data[((((b*d.Cout+oc)*oD+od)*oH+oy)*oW+ox)]
                              * Wt.data[((((oc*d.Cin+ic)*d.kD+kz)*d.kH+ky)*d.kW+kx)];
                }
            }
        }
        DX.data[e] = acc;
    }
}
)NKSL";
		// ---- Conv3D dW : gather par poids [Cout,Cin,kD,kH,kW] ----------------------
		static const char *kConv3dDwNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufX { float data[]; } X;
@binding(set=0, binding=2) buffer BufD { float data[]; } DW;
@binding(set=0, binding=3) uniform P { uint B; uint Cin; uint D; uint H; uint W; uint Cout; uint kD; uint kH; uint kW; uint stride; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint oD = (d.D + 2u*d.pad - d.kD)/d.stride + 1u;
    uint oH = (d.H + 2u*d.pad - d.kH)/d.stride + 1u;
    uint oW = (d.W + 2u*d.pad - d.kW)/d.stride + 1u;
    uint e = gl_GlobalInvocationID.x;
    uint total = d.Cout * d.Cin * d.kD * d.kH * d.kW;
    if (e < total) {
        uint kx = e % d.kW; uint t = e / d.kW; uint ky = t % d.kH; t = t / d.kH;
        uint kz = t % d.kD; t = t / d.kD; uint ic = t % d.Cin; uint oc = t / d.Cin;
        float acc = 0.0;
        for (uint b = 0u; b < d.B; b = b + 1u)
        for (uint od = 0u; od < oD; od = od + 1u)
        for (uint oy = 0u; oy < oH; oy = oy + 1u)
        for (uint ox = 0u; ox < oW; ox = ox + 1u) {
            int iz = int(od)*int(d.stride) - int(d.pad) + int(kz);
            int iy = int(oy)*int(d.stride) - int(d.pad) + int(ky);
            int ix = int(ox)*int(d.stride) - int(d.pad) + int(kx);
            if (iz>=0 && iz<int(d.D) && iy>=0 && iy<int(d.H) && ix>=0 && ix<int(d.W)) {
                acc = acc + X.data[((((b*d.Cin+ic)*d.D+uint(iz))*d.H+uint(iy))*d.W+uint(ix))]
                          * G.data[((((b*d.Cout+oc)*oD+od)*oH+oy)*oW+ox)];
            }
        }
        DW.data[e] = acc;
    }
}
)NKSL";
		// ---- ConvTranspose3D forward : gather par sortie ; w[Cin,Cout,kD,kH,kW] ----
		static const char *kConvT3dFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wt;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform P { uint B; uint Cin; uint D; uint H; uint W; uint Cout; uint kD; uint kH; uint kW; uint stride; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint oD = (d.D-1u)*d.stride - 2u*d.pad + d.kD;
    uint oH = (d.H-1u)*d.stride - 2u*d.pad + d.kH;
    uint oW = (d.W-1u)*d.stride - 2u*d.pad + d.kW;
    uint i = gl_GlobalInvocationID.x;
    uint total = d.B * d.Cout * oD * oH * oW;
    if (i < total) {
        uint ox = i % oW; uint t = i / oW; uint oy = t % oH; t = t / oH;
        uint od = t % oD; t = t / oD; uint oc = t % d.Cout; uint b = t / d.Cout;
        float acc = 0.0;
        for (uint kz = 0u; kz < d.kD; kz = kz + 1u)
        for (uint ky = 0u; ky < d.kH; ky = ky + 1u)
        for (uint kx = 0u; kx < d.kW; kx = kx + 1u) {
            int izn = int(od) + int(d.pad) - int(kz);
            int iyn = int(oy) + int(d.pad) - int(ky);
            int ixn = int(ox) + int(d.pad) - int(kx);
            bool ok = (izn>=0)&&(uint(izn)%d.stride==0u) && (iyn>=0)&&(uint(iyn)%d.stride==0u) && (ixn>=0)&&(uint(ixn)%d.stride==0u);
            if (ok) {
                uint iz = uint(izn)/d.stride; uint iy = uint(iyn)/d.stride; uint ix = uint(ixn)/d.stride;
                if (iz<d.D && iy<d.H && ix<d.W) {
                    for (uint ic = 0u; ic < d.Cin; ic = ic + 1u) {
                        acc = acc + X.data[((((b*d.Cin+ic)*d.D+iz)*d.H+iy)*d.W+ix)]
                                  * Wt.data[((((ic*d.Cout+oc)*d.kD+kz)*d.kH+ky)*d.kW+kx)];
                    }
                }
            }
        }
        Y.data[i] = acc;
    }
}
)NKSL";
		// ---- ConvTranspose3D dX : gather par entrée --------------------------------
		static const char *kConvT3dDxNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wt;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P { uint B; uint Cin; uint D; uint H; uint W; uint Cout; uint kD; uint kH; uint kW; uint stride; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint oD = (d.D-1u)*d.stride - 2u*d.pad + d.kD;
    uint oH = (d.H-1u)*d.stride - 2u*d.pad + d.kH;
    uint oW = (d.W-1u)*d.stride - 2u*d.pad + d.kW;
    uint e = gl_GlobalInvocationID.x;
    uint total = d.B * d.Cin * d.D * d.H * d.W;
    if (e < total) {
        uint ix = e % d.W; uint t = e / d.W; uint iy = t % d.H; t = t / d.H;
        uint iz = t % d.D; t = t / d.D; uint ic = t % d.Cin; uint b = t / d.Cin;
        float acc = 0.0;
        for (uint oc = 0u; oc < d.Cout; oc = oc + 1u)
        for (uint kz = 0u; kz < d.kD; kz = kz + 1u)
        for (uint ky = 0u; ky < d.kH; ky = ky + 1u)
        for (uint kx = 0u; kx < d.kW; kx = kx + 1u) {
            int od = int(iz)*int(d.stride) - int(d.pad) + int(kz);
            int oy = int(iy)*int(d.stride) - int(d.pad) + int(ky);
            int ox = int(ix)*int(d.stride) - int(d.pad) + int(kx);
            if (od>=0 && od<int(oD) && oy>=0 && oy<int(oH) && ox>=0 && ox<int(oW)) {
                acc = acc + G.data[((((b*d.Cout+oc)*oD+uint(od))*oH+uint(oy))*oW+uint(ox))]
                          * Wt.data[((((ic*d.Cout+oc)*d.kD+kz)*d.kH+ky)*d.kW+kx)];
            }
        }
        DX.data[e] = acc;
    }
}
)NKSL";
		// ---- ConvTranspose3D dW : gather par poids [Cin,Cout,kD,kH,kW] -------------
		static const char *kConvT3dDwNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufX { float data[]; } X;
@binding(set=0, binding=2) buffer BufD { float data[]; } DW;
@binding(set=0, binding=3) uniform P { uint B; uint Cin; uint D; uint H; uint W; uint Cout; uint kD; uint kH; uint kW; uint stride; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint oD = (d.D-1u)*d.stride - 2u*d.pad + d.kD;
    uint oH = (d.H-1u)*d.stride - 2u*d.pad + d.kH;
    uint oW = (d.W-1u)*d.stride - 2u*d.pad + d.kW;
    uint e = gl_GlobalInvocationID.x;
    uint total = d.Cin * d.Cout * d.kD * d.kH * d.kW;
    if (e < total) {
        uint kx = e % d.kW; uint t = e / d.kW; uint ky = t % d.kH; t = t / d.kH;
        uint kz = t % d.kD; t = t / d.kD; uint oc = t % d.Cout; uint ic = t / d.Cout;
        float acc = 0.0;
        for (uint b = 0u; b < d.B; b = b + 1u)
        for (uint iz = 0u; iz < d.D; iz = iz + 1u)
        for (uint iy = 0u; iy < d.H; iy = iy + 1u)
        for (uint ix = 0u; ix < d.W; ix = ix + 1u) {
            int od = int(iz)*int(d.stride) - int(d.pad) + int(kz);
            int oy = int(iy)*int(d.stride) - int(d.pad) + int(ky);
            int ox = int(ix)*int(d.stride) - int(d.pad) + int(kx);
            if (od>=0 && od<int(oD) && oy>=0 && oy<int(oH) && ox>=0 && ox<int(oW)) {
                acc = acc + X.data[((((b*d.Cin+ic)*d.D+iz)*d.H+iy)*d.W+ix)]
                          * G.data[((((b*d.Cout+oc)*oD+uint(od))*oH+uint(oy))*oW+uint(ox))];
            }
        }
        DW.data[e] = acc;
    }
}
)NKSL";

		NkTensor NkGpuConv3D(const NkTensor &x, const NkTensor &w, int64 stride, int64 pad) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gw = (w.Device() == NkDevice::NK_GPU) ? w : w.ToGPU();
			if (!gx.IsValid() || !gw.IsValid() || gx.Rank() != 5 || gw.Rank() != 5)
				return NkTensor{};
			const int64 B = gx.Shape()[0], Cin = gx.Shape()[1], D = gx.Shape()[2], H = gx.Shape()[3], W = gx.Shape()[4];
			const int64 Cout = gw.Shape()[0], kD = gw.Shape()[2], kH = gw.Shape()[3], kW = gw.Shape()[4];
			const int64 oD = (D + 2 * pad - kD) / stride + 1, oH = (H + 2 * pad - kH) / stride + 1,
						oW = (W + 2 * pad - kW) / stride + 1;
			const int64 no = B * Cout * oD * oH * oW;
			uint64 yb = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(gx.DType()));
			if (!yb)
				return NkTensor{};
			uint32 p[12];
			Conv3dParams(p, B, Cin, D, H, W, Cout, kD, kH, kW, stride, pad);
			NkTensorGpu::Get().RunOp3("conv3d_fwd", NkString(kConv3dFwdNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gw), yb, p, (uint32)no);
			return NkTensorInternal::MakeGpu(NkShape{B, Cout, oD, oH, oW}, gx.DType(), yb);
		}

		NkTensor NkGpuConv3DBackwardX(const NkTensor &grad, const NkTensor &w, const NkTensor &x, int64 stride,
									  int64 pad) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor gw = (w.Device() == NkDevice::NK_GPU) ? w : w.ToGPU();
			if (!gg.IsValid() || !gw.IsValid() || x.Rank() != 5)
				return NkTensor{};
			const int64 B = x.Shape()[0], Cin = x.Shape()[1], D = x.Shape()[2], H = x.Shape()[3], W = x.Shape()[4];
			const int64 Cout = gw.Shape()[0], kD = gw.Shape()[2], kH = gw.Shape()[3], kW = gw.Shape()[4];
			const int64 ni = B * Cin * D * H * W;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)ni * NkDTypeSize(gg.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12];
			Conv3dParams(p, B, Cin, D, H, W, Cout, kD, kH, kW, stride, pad);
			NkTensorGpu::Get().RunOp3("conv3d_dx", NkString(kConv3dDxNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gw), db, p, (uint32)ni);
			return NkTensorInternal::MakeGpu(NkShape{B, Cin, D, H, W}, gg.DType(), db);
		}

		NkTensor NkGpuConv3DBackwardW(const NkTensor &grad, const NkTensor &x, const NkTensor &w, int64 stride,
									  int64 pad) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gg.IsValid() || !gx.IsValid() || w.Rank() != 5)
				return NkTensor{};
			const int64 B = gx.Shape()[0], Cin = gx.Shape()[1], D = gx.Shape()[2], H = gx.Shape()[3], W = gx.Shape()[4];
			const int64 Cout = w.Shape()[0], kD = w.Shape()[2], kH = w.Shape()[3], kW = w.Shape()[4];
			const int64 nw = Cout * Cin * kD * kH * kW;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)nw * NkDTypeSize(gx.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12];
			Conv3dParams(p, B, Cin, D, H, W, Cout, kD, kH, kW, stride, pad);
			NkTensorGpu::Get().RunOp3("conv3d_dw", NkString(kConv3dDwNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gx), db, p, (uint32)nw);
			return NkTensorInternal::MakeGpu(NkShape{Cout, Cin, kD, kH, kW}, gx.DType(), db);
		}

		NkTensor NkGpuConvTranspose3D(const NkTensor &x, const NkTensor &w, int64 stride, int64 pad) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gw = (w.Device() == NkDevice::NK_GPU) ? w : w.ToGPU();
			if (!gx.IsValid() || !gw.IsValid() || gx.Rank() != 5 || gw.Rank() != 5)
				return NkTensor{};
			const int64 B = gx.Shape()[0], Cin = gx.Shape()[1], D = gx.Shape()[2], H = gx.Shape()[3], W = gx.Shape()[4];
			const int64 Cout = gw.Shape()[1], kD = gw.Shape()[2], kH = gw.Shape()[3], kW = gw.Shape()[4];
			const int64 oD = (D - 1) * stride - 2 * pad + kD, oH = (H - 1) * stride - 2 * pad + kH,
						oW = (W - 1) * stride - 2 * pad + kW;
			const int64 no = B * Cout * oD * oH * oW;
			uint64 yb = NkTensorGpu::Get().CreateBuffer((nk_size)no * NkDTypeSize(gx.DType()));
			if (!yb)
				return NkTensor{};
			uint32 p[12];
			Conv3dParams(p, B, Cin, D, H, W, Cout, kD, kH, kW, stride, pad);
			NkTensorGpu::Get().RunOp3("convt3d_fwd", NkString(kConvT3dFwdNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gw), yb, p, (uint32)no);
			return NkTensorInternal::MakeGpu(NkShape{B, Cout, oD, oH, oW}, gx.DType(), yb);
		}

		NkTensor NkGpuConvTranspose3DBackwardX(const NkTensor &grad, const NkTensor &w, const NkTensor &x, int64 stride,
											   int64 pad) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor gw = (w.Device() == NkDevice::NK_GPU) ? w : w.ToGPU();
			if (!gg.IsValid() || !gw.IsValid() || x.Rank() != 5)
				return NkTensor{};
			const int64 B = x.Shape()[0], Cin = x.Shape()[1], D = x.Shape()[2], H = x.Shape()[3], W = x.Shape()[4];
			const int64 Cout = gw.Shape()[1], kD = gw.Shape()[2], kH = gw.Shape()[3], kW = gw.Shape()[4];
			const int64 ni = B * Cin * D * H * W;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)ni * NkDTypeSize(gg.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12];
			Conv3dParams(p, B, Cin, D, H, W, Cout, kD, kH, kW, stride, pad);
			NkTensorGpu::Get().RunOp3("convt3d_dx", NkString(kConvT3dDxNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gw), db, p, (uint32)ni);
			return NkTensorInternal::MakeGpu(NkShape{B, Cin, D, H, W}, gg.DType(), db);
		}

		NkTensor NkGpuConvTranspose3DBackwardW(const NkTensor &grad, const NkTensor &x, const NkTensor &w, int64 stride,
											   int64 pad) {
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gg.IsValid() || !gx.IsValid() || w.Rank() != 5)
				return NkTensor{};
			const int64 B = gx.Shape()[0], Cin = gx.Shape()[1], D = gx.Shape()[2], H = gx.Shape()[3], W = gx.Shape()[4];
			const int64 Cout = w.Shape()[1], kD = w.Shape()[2], kH = w.Shape()[3], kW = w.Shape()[4];
			const int64 nw = Cin * Cout * kD * kH * kW;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)nw * NkDTypeSize(gx.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12];
			Conv3dParams(p, B, Cin, D, H, W, Cout, kD, kH, kW, stride, pad);
			NkTensorGpu::Get().RunOp3("convt3d_dw", NkString(kConvT3dDwNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gx), db, p, (uint32)nw);
			return NkTensorInternal::MakeGpu(NkShape{Cin, Cout, kD, kH, kW}, gx.DType(), db);
		}

		NkTensor NkTensor::ToGPU() const {
			if (mDevice == NkDevice::NK_GPU)
				return *this;
			if (!NkTensorGpu::Get().IsAvailable())
				return NkTensor{};
			NkTensor cont = Contiguous();
			const nk_size bytes = (nk_size)cont.Numel() * NkDTypeSize(cont.DType());
			uint64 buf = NkTensorGpu::Get().CreateBuffer(bytes);
			if (!buf)
				return NkTensor{};
			NkTensorGpu::Get().Upload(buf, cont.RawData(), bytes);
			return NkTensorInternal::MakeGpu(cont.Shape(), cont.DType(), buf);
		}

		NkTensor NkTensor::ToCPU() const {
			if (mDevice == NkDevice::NK_CPU)
				return *this;
			NkTensor out = NkTensor::Empty(mShape, mDType, NkDevice::NK_CPU);
			const nk_size bytes = (nk_size)Numel() * NkDTypeSize(mDType);
			NkTensorGpu::Get().Download(mStorage->gpuBuffer, out.RawData(), bytes);
			return out;
		}

	} // namespace ai
} // namespace nkentseu
