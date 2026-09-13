// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGpuAtomicWitness.cpp — voir le .h. Chaîne de compilation : celle de NkIBLCompute
// (NkSL -> GLSL, puis HLSL / SPIR-V / MSL par SPIRV-Cross selon le device).
// =============================================================================
#include "NkGpuAtomicWitness.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include <cstdio>

namespace nkentseu {
	namespace renderer {

		static const char *kAtomicNkSL = R"NKSL(
@binding(set=0, binding=0) buffer Cnt { uint c[]; } C;
@binding(set=0, binding=1) uniform Params { uint n; uint pad0; uint pad1; uint pad2; } p;
layout(local_size_x = 256) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.n) {
        atomicAdd(C.c[0], 1u);
        atomicMax(C.c[1], i);
    }
}
)NKSL";

		// La mutation : le même noyau sans atomique -- lecture, +1, écriture.
		static const char *kMutantNkSL = R"NKSL(
@binding(set=0, binding=0) buffer Cnt { uint c[]; } C;
@binding(set=0, binding=1) uniform Params { uint n; uint pad0; uint pad1; uint pad2; } p;
layout(local_size_x = 256) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < p.n) {
        C.c[0] = C.c[0] + 1u;
        if (i > C.c[1]) { C.c[1] = i; }
    }
}
)NKSL";

		NkGpuAtomicResult NkGpuAtomicWitness(NkIDevice *device, uint32 invocations, bool mutation) {
			NkGpuAtomicResult res;
			if (!device) {
				res.why = "pas de device";
				return res;
			}
			if (!device->GetCaps().computeShaders) {
				res.why = "pas de compute sur ce device";
				return res;
			}
			NkString src(mutation ? kMutantNkSL : kAtomicNkSL);
			NkSLCompiler slc;
			NkSLCompileResult gl = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
			if (!gl.success) {
				res.why = "NkSL -> GLSL refuse";
				for (uint32 i = 0; i < gl.errors.Size() && i < 3; ++i)
					std::fprintf(stderr, "[NkGpuAtomicWitness] ligne %u : %s\n", gl.errors[i].line, gl.errors[i].message.CStr());
				return res;
			}
			NkShaderConvertResult hl, sp, ms;
			NkSLCompileResult glo;
			NkShaderDesc sd;
			sd.debugName = "atomic_witness";
			const NkGraphicsApi api = device->GetApi();
			if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
				hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, 50u, "atomic_witness");
				if (!hl.success) {
					res.why = "GLSL -> HLSL refuse";
					return res;
				}
				sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
				sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, "atomic_witness");
				if (!sp.success) {
					res.why = "GLSL -> SPIR-V refuse";
					return res;
				}
				sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
			} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
				ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, "atomic_witness");
				if (!ms.success) {
					res.why = "GLSL -> MSL refuse";
					return res;
				}
				sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
				glo = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
				if (!glo.success) {
					res.why = "NkSL -> GLSL(GL) refuse";
					return res;
				}
				sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
			} else {
				res.why = "API sans compute";
				return res;
			}
			::nkentseu::NkShaderHandle shader = device->CreateShader(sd);
			if (!shader.IsValid()) {
				res.why = "CreateShader refuse";
				return res;
			}
			NkDescriptorSetLayoutDesc ld;
			ld.Add(0, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			NkDescSetHandle layout = device->CreateDescriptorSetLayout(ld);
			NkComputePipelineDesc cpd;
			cpd.shader = shader;
			cpd.debugName = "atomic_witness";
			cpd.descriptorSetLayouts.PushBack(layout);
			NkPipelineHandle pipe = device->CreateComputePipeline(cpd);
			if (!pipe.IsValid()) {
				res.why = "CreateComputePipeline refuse";
				return res;
			}
			const uint32 zeros[4] = {0, 0, 0, 0};
			NkBufferDesc bd = NkBufferDesc::Storage(16, true);
			bd.initialData = zeros;
			bd.debugName = "atomic_counter";
			NkBufferHandle cnt = device->CreateBuffer(bd);
			NkBufferHandle ubo = device->CreateBuffer(NkBufferDesc::Uniform(16));
			const uint32 params[4] = {invocations, 0, 0, 0};
			device->WriteBuffer(ubo, params, 16);
			device->WriteBuffer(cnt, zeros, 16);
			NkDescSetHandle set = device->AllocateDescriptorSet(layout);
			{
				NkDescriptorWrite w{};
				w.set = set;
				w.binding = 0;
				w.type = NkDescriptorType::NK_STORAGE_BUFFER;
				w.buffer = cnt;
				device->UpdateDescriptorSets(&w, 1);
			}
			device->BindUniformBuffer(set, 1, ubo);
			NkICommandBuffer *cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(pipe);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((invocations + 255u) / 256u, 1, 1);
			cmd->UAVBarrier(cnt);
			cmd->End();
			device->Submit(&cmd, 1);
			device->WaitIdle();
			uint32 out[4] = {0, 0, 0, 0};
			device->ReadBuffer(cnt, out, 16, 0);
			res.ran = true;
			res.add = out[0];
			res.max = out[1];
			device->DestroyCommandBuffer(cmd);
			device->FreeDescriptorSet(set);
			device->DestroyBuffer(cnt);
			device->DestroyBuffer(ubo);
			return res;
		}

	} // namespace renderer
} // namespace nkentseu
