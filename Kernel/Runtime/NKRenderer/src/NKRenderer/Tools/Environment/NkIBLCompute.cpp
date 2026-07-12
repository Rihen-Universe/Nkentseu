// =============================================================================
// NkIBLCompute.cpp — Phase N v1 : convolutions IBL sur GPU (compute).
//
// Les deux kernels NkSL répliquent EXACTEMENT les boucles CPU de
// NkEnvironmentSystem.cpp (CubemapFaceUVToDir, SampleEquirect nearest,
// BuildTBN, Hammersley, ImportanceSampleGGX, Reinhard, packing RGBA8) —
// même sortie au bit de quantification près (trig float GPU vs CPU).
//
// Chemin de compilation copié de NkTensorGpu (prouvé VK/GL/DX11/DX12) :
// NkSL → GLSL-Vulkan → glslang/SPIRV-Cross → SPIRV/GLSL/HLSL → pipeline.
// =============================================================================
#include "NkIBLCompute.h"

#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace renderer {

		// ── Paramètres partagés par les deux kernels (UBO binding 2) ────────
		// std140 : 8 scalaires 32-bit = 32 octets, pas de padding surprise.
		struct IBLParams {
				uint32 hdrW;
				uint32 hdrH;
				uint32 outSize;
				uint32 count; // outSize*outSize*6
				float32 roughness;
				uint32 numSamples;
				uint32 pad0;
				uint32 pad1;
		};

		// ── Préambule commun : buffers + helpers (mêmes formules que le CPU) ─
		// NB dialecte : boucles `i = i + 1u`, pas de return anticipé dans main
		// (corps sous `if (idx < count)`), helpers purs + accès buffer au
		// niveau helper (comme les shaders PBR migrés).
		static const char *kIBLCommonNkSL = R"NKSL(
@binding(set=0, binding=0) buffer HdrBuf { vec4 texel[]; } H;
@binding(set=0, binding=1) buffer OutBuf { uint px[]; } O;
@binding(set=0, binding=2) uniform Params {
    uint hdrW; uint hdrH; uint outSize; uint count;
    float roughness; uint numSamples; uint pad0; uint pad1;
} p;
layout(local_size_x = 64) in;

// Direction monde du texel (face, u, v) — mêmes conventions que le CPU.
vec3 faceUVToDir(uint face, float u, float v) {
    vec3 d = vec3(0.0, 1.0, 0.0);
    if (face == 0u) { d = vec3( 1.0, -v, -u); }
    if (face == 1u) { d = vec3(-1.0, -v,  u); }
    if (face == 2u) { d = vec3(  u, 1.0,  v); }
    if (face == 3u) { d = vec3(  u,-1.0, -v); }
    if (face == 4u) { d = vec3(  u,  -v, 1.0); }
    if (face == 5u) { d = vec3( -u,  -v,-1.0); }
    return normalize(d);
}

// Équirect nearest — réplique SampleEquirect (atan2(dz,dx), asin(dy)).
vec3 sampleEquirect(vec3 d) {
    float invPI  = 0.31830988618379;
    float inv2PI = 0.15915494309189;
    float phi = atan(d.z, d.x);
    float theta = asin(clamp(d.y, -1.0, 1.0));
    float u = phi * inv2PI + 0.5;
    float v = 0.5 - theta * invPI;
    int px = int(u * float(p.hdrW));
    int py = int(v * float(p.hdrH));
    px = clamp(px, 0, int(p.hdrW) - 1);
    py = clamp(py, 0, int(p.hdrH) - 1);
    uint idx = uint(py) * p.hdrW + uint(px);
    return H.texel[idx].xyz;
}

// Reinhard + clamp + packing RGBA8 little-endian (r = octet 0).
uint packReinhard(vec3 c) {
    c = c / (vec3(1.0, 1.0, 1.0) + c);
    c = clamp(c, 0.0, 1.0);
    uint r = uint(c.x * 255.0);
    uint g = uint(c.y * 255.0);
    uint b = uint(c.z * 255.0);
    return r | (g << 8u) | (b << 16u) | (255u << 24u);
}
)NKSL";

		// ── Kernel 1 : irradiance Lambert (4 strates × 16 azimuts) ──────────
		static const char *kIrradianceNkSL = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < p.count) {
        uint faceTexels = p.outSize * p.outSize;
        uint face = idx / faceTexels;
        uint rem = idx - face * faceTexels;
        uint x = rem - (rem / p.outSize) * p.outSize;
        uint y = rem / p.outSize;

        float u = (float(x) + 0.5) / float(p.outSize) * 2.0 - 1.0;
        float v = (float(y) + 0.5) / float(p.outSize) * 2.0 - 1.0;
        vec3 N = faceUVToDir(face, u, v);

        // BuildTBN : up = |N.y| < 0.999 ? (0,1,0) : (1,0,0)
        vec3 up = vec3(1.0, 0.0, 0.0);
        if (abs(N.y) < 0.999) { up = vec3(0.0, 1.0, 0.0); }
        vec3 T = normalize(cross(up, N));
        vec3 B = cross(N, T);

        float kPI = 3.14159265358979;
        uint nTheta = 4u;
        uint nPhi = 16u;
        float dTheta = 0.5 * kPI / float(nTheta);
        float dPhi = 2.0 * kPI / float(nPhi);

        vec3 c = vec3(0.0, 0.0, 0.0);
        uint nSamp = 0u;
        for (uint ti = 0u; ti < nTheta; ti = ti + 1u) {
            float theta = (float(ti) + 0.5) * dTheta;
            float sT = sin(theta);
            float cT = cos(theta);
            for (uint pi = 0u; pi < nPhi; pi = pi + 1u) {
                float phi = (float(pi) + 0.5) * dPhi;
                vec3 l = vec3(sT * cos(phi), sT * sin(phi), cT);
                vec3 W = T * l.x + B * l.y + N * l.z;
                c = c + sampleEquirect(W) * (cT * sT);
                nSamp = nSamp + 1u;
            }
        }
        c = c * (kPI / float(nSamp));
        O.px[idx] = packReinhard(c);
    }
}
)NKSL";

		// ── Kernel 2 : prefilter GGX (Hammersley, numSamples) ───────────────
		static const char *kPrefilterNkSL = R"NKSL(
// Van der Corput radical inverse (base 2) — réplique RadicalInverseVdC.
float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec3 importanceSampleGGX(float xiX, float xiY, float roughness, vec3 N) {
    float a = roughness * roughness;
    float phi = 6.28318530718 * xiX;
    float cosTheta = sqrt((1.0 - xiY) / (1.0 + (a * a - 1.0) * xiY));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    vec3 l = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    vec3 up = vec3(1.0, 0.0, 0.0);
    if (abs(N.y) < 0.999) { up = vec3(0.0, 1.0, 0.0); }
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    return normalize(T * l.x + B * l.y + N * l.z);
}

@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < p.count) {
        uint faceTexels = p.outSize * p.outSize;
        uint face = idx / faceTexels;
        uint rem = idx - face * faceTexels;
        uint x = rem - (rem / p.outSize) * p.outSize;
        uint y = rem / p.outSize;

        float u = (float(x) + 0.5) / float(p.outSize) * 2.0 - 1.0;
        float v = (float(y) + 0.5) / float(p.outSize) * 2.0 - 1.0;
        vec3 N = faceUVToDir(face, u, v);
        vec3 V = N;

        vec3 c = vec3(0.0, 0.0, 0.0);
        if (p.roughness < 0.001) {
            // Mip 0 : mirror direct du HDR (mêmes octets que le CPU).
            c = sampleEquirect(N);
        } else {
            float sumW = 0.0;
            for (uint i = 0u; i < p.numSamples; i = i + 1u) {
                float xiX = float(i) / float(p.numSamples);
                float xiY = radicalInverseVdC(i);
                vec3 Hn = importanceSampleGGX(xiX, xiY, p.roughness, N);
                float VoH = dot(V, Hn);
                vec3 L = Hn * (2.0 * VoH) - V;
                float NoL = max(dot(N, L), 0.0);
                if (NoL > 0.0) {
                    c = c + sampleEquirect(L) * NoL;
                    sumW = sumW + NoL;
                }
            }
            if (sumW > 0.000001) { c = c * (1.0 / sumW); }
            else { c = vec3(0.0, 0.0, 0.0); }
        }
        O.px[idx] = packReinhard(c);
    }
}
)NKSL";

		// =====================================================================
		// Cycle de vie
		// =====================================================================

		NkIBLCompute::~NkIBLCompute() {
			Shutdown();
		}

		bool NkIBLCompute::CompileKernel(const char *name, const char *nkslSrc, Kernel &out) {
			// Assemble préambule commun + corps du kernel.
			NkString src(kIBLCommonNkSL);
			src.Append(nkslSrc);

			NkSLCompiler slc;
			NkSLCompileResult gl = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
			if (!gl.success) {
				logger.Warnf("[NkIBLCompute] NkSL->GLSL KO (%s) — fallback CPU\n", name);
				return false;
			}

			// Les sources doivent rester vivantes jusqu'après CreateShader
			// (NkShaderStageDesc garde des const char* non possédés).
			NkShaderConvertResult hl, sp, ms;
			NkSLCompileResult glo;
			NkShaderDesc sd;
			sd.debugName = name;
			const NkGraphicsApi api = mDevice->GetApi();
			if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
				// SM5.0 pour DX11 ET DX12 (chemin fxc prouvé par NkTensorGpu).
				hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, 50u, name);
				if (!hl.success) {
					logger.Warnf("[NkIBLCompute] GLSL->HLSL KO (%s) — fallback CPU\n", name);
					return false;
				}
				sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
				sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, name);
				if (!sp.success) {
					logger.Warnf("[NkIBLCompute] GLSL->SPIRV KO (%s) — fallback CPU\n", name);
					return false;
				}
				sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
			} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
				ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, name);
				if (!ms.success) {
					logger.Warnf("[NkIBLCompute] GLSL->MSL KO (%s) — fallback CPU\n", name);
					return false;
				}
				sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
			} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
				glo = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
				if (!glo.success) {
					logger.Warnf("[NkIBLCompute] NkSL->GLSL(GL) KO (%s) — fallback CPU\n", name);
					return false;
				}
				sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
			} else {
				// Software / API sans compute : fallback CPU silencieux.
				return false;
			}

			out.shader = mDevice->CreateShader(sd);
			if (!out.shader.IsValid()) {
				logger.Warnf("[NkIBLCompute] CreateShader KO (%s) — fallback CPU\n", name);
				return false;
			}

			NkDescriptorSetLayoutDesc ld;
			ld.Add(0, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(1, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(2, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			out.layout = mDevice->CreateDescriptorSetLayout(ld);

			NkComputePipelineDesc cpd;
			cpd.shader = out.shader;
			cpd.debugName = name;
			cpd.descriptorSetLayouts.PushBack(out.layout);
			out.pipeline = mDevice->CreateComputePipeline(cpd);
			if (!out.pipeline.IsValid()) {
				logger.Warnf("[NkIBLCompute] Pipeline KO (%s) — fallback CPU\n", name);
				return false;
			}
			return true;
		}

		bool NkIBLCompute::Init(NkIDevice *device) {
			if (device == nullptr)
				return false;
			mDevice = device;

			if (!CompileKernel("ibl_irradiance", kIrradianceNkSL, mIrradiance))
				return false;
			if (!CompileKernel("ibl_prefilter", kPrefilterNkSL, mPrefilter))
				return false;

			mParamsBuffer = mDevice->CreateBuffer(NkBufferDesc::Uniform(64));
			if (!mParamsBuffer.IsValid())
				return false;

			mReady = true;
			return true;
		}

		void NkIBLCompute::Shutdown() {
			if (mDevice == nullptr)
				return;
			if (mHdrBuffer.IsValid())
				mDevice->DestroyBuffer(mHdrBuffer);
			if (mOutBuffer.IsValid())
				mDevice->DestroyBuffer(mOutBuffer);
			if (mParamsBuffer.IsValid())
				mDevice->DestroyBuffer(mParamsBuffer);
			mHdrBuffer = {};
			mOutBuffer = {};
			mParamsBuffer = {};
			mOutCapacity = 0;
			// Shaders/pipelines : détruits avec le device (durée de vie courte
			// de l'objet, utilisé au chargement seulement).
			mReady = false;
			mDevice = nullptr;
		}

		bool NkIBLCompute::UploadHDR(const float *rgbaPixels, uint32 width, uint32 height) {
			if (!mReady || rgbaPixels == nullptr || width == 0 || height == 0)
				return false;
			const uint64 bytes = (uint64)width * (uint64)height * 4u * sizeof(float32);
			mHdrBuffer = mDevice->CreateBuffer(NkBufferDesc::Storage(bytes, false));
			if (!mHdrBuffer.IsValid())
				return false;
			if (!mDevice->WriteBuffer(mHdrBuffer, rgbaPixels, bytes))
				return false;
			mHdrWidth = width;
			mHdrHeight = height;
			return true;
		}

		bool NkIBLCompute::RunKernel(const Kernel &kernel, uint32 outSize, float32 roughness, uint32 numSamples,
									 uint8 *outFaces[6]) {
			const uint32 count = outSize * outSize * 6u;
			const uint64 outBytes = (uint64)count * sizeof(uint32);

			// (Ré)alloue le buffer de sortie si trop petit.
			if (!mOutBuffer.IsValid() || mOutCapacity < outBytes) {
				if (mOutBuffer.IsValid())
					mDevice->DestroyBuffer(mOutBuffer);
				mOutBuffer = mDevice->CreateBuffer(NkBufferDesc::Storage(outBytes, false));
				if (!mOutBuffer.IsValid())
					return false;
				mOutCapacity = outBytes;
			}

			IBLParams params{};
			params.hdrW = mHdrWidth;
			params.hdrH = mHdrHeight;
			params.outSize = outSize;
			params.count = count;
			params.roughness = roughness;
			params.numSamples = numSamples;
			if (!mDevice->WriteBuffer(mParamsBuffer, &params, sizeof(params)))
				return false;

			NkDescSetHandle set = mDevice->AllocateDescriptorSet(kernel.layout);
			{
				NkDescriptorWrite w{};
				w.set = set;
				w.binding = 0;
				w.type = NkDescriptorType::NK_STORAGE_BUFFER;
				w.buffer = mHdrBuffer;
				mDevice->UpdateDescriptorSets(&w, 1);
				w.binding = 1;
				w.buffer = mOutBuffer;
				mDevice->UpdateDescriptorSets(&w, 1);
			}
			mDevice->BindUniformBuffer(set, 2, mParamsBuffer);

			auto *cmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			cmd->Begin();
			cmd->BindComputePipeline(kernel.pipeline);
			cmd->BindDescriptorSet(set, 0);
			cmd->Dispatch((count + 63u) / 64u, 1, 1);
			cmd->UAVBarrier(mOutBuffer);
			cmd->End();
			mDevice->Submit(&cmd, 1);
			mDevice->WaitIdle();
			mDevice->FreeDescriptorSet(set);
			mDevice->DestroyCommandBuffer(cmd);

			// Readback face-major : out[face*outSize² + y*outSize + x], packing
			// RGBA8 little-endian identique aux octets CPU → copie directe.
			const uint32 faceBytes = outSize * outSize * 4u;
			for (uint32 f = 0; f < 6u; ++f) {
				if (outFaces[f] == nullptr)
					return false;
				if (!mDevice->ReadBuffer(mOutBuffer, outFaces[f], faceBytes, (uint64)f * faceBytes))
					return false;
			}
			return true;
		}

		bool NkIBLCompute::ConvolveIrradiance(uint32 irrSize, uint8 *outFaces[6]) {
			if (!mReady || !mHdrBuffer.IsValid() || irrSize == 0)
				return false;
			return RunKernel(mIrradiance, irrSize, 0.f, 0, outFaces);
		}

		bool NkIBLCompute::PrefilterMip(uint32 mipSize, float32 roughness, uint32 numSamples, uint8 *outFaces[6]) {
			if (!mReady || !mHdrBuffer.IsValid() || mipSize == 0)
				return false;
			return RunKernel(mPrefilter, mipSize, roughness, numSamples, outFaces);
		}

	} // namespace renderer
} // namespace nkentseu
