#pragma once
// =============================================================================
// NkIBLCompute.h — NKRenderer (Tools/Environment/)
//
// Phase N v1 : convolutions IBL sur GPU (compute).
//   - Irradiance cubemap : convolution Lambert (4 strates × 16 azimuts)
//   - Prefilter cubemap  : GGX importance sampling (Hammersley), par mip
//
// Réplique EXACTEMENT le chemin CPU de NkEnvironmentSystem (mêmes formules,
// même échantillonnage equirect nearest, même Reinhard, même packing RGBA8)
// pour que la sortie soit interchangeable : mêmes buffers → même upload
// (WriteTextureRegion) et même cache disque (nk_ibl_*.bin) inchangés.
//
// Chemin compute PROUVÉ (copié de NkTensorGpu / NkComputeNkSL) :
//   NkSL → GLSL-Vulkan → glslang/SPIRV-Cross → SPIRV/GLSL/HLSL → pipeline.
// Fonctionne sur Vulkan / OpenGL / DX11 / DX12. Sur échec (backend sans
// compute, compile KO), l'appelant retombe sur la convolution CPU.
//
// Usage (depuis NkEnvironmentSystem::LoadFromHDR) :
//   NkIBLCompute gpu;
//   if (gpu.Init(device) && gpu.UploadHDR(rgba, w, h)) {
//       gpu.ConvolveIrradiance(irrSize, outFaces6);          // 6× RGBA8
//       gpu.PrefilterMip(mipSize, roughness, 32, outFaces6); // par mip
//   }
//   gpu.Shutdown(); // ou destructeur
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
	namespace renderer {

		class NkIBLCompute {
			public:
				NkIBLCompute() = default;
				~NkIBLCompute();

				NkIBLCompute(const NkIBLCompute &) = delete;
				NkIBLCompute &operator=(const NkIBLCompute &) = delete;

				// Compile les deux kernels compute pour le backend du device.
				// Renvoie false si le backend n'a pas de compute utilisable
				// (l'appelant garde alors le chemin CPU).
				bool Init(NkIDevice *device);
				void Shutdown();

				bool IsReady() const {
					return mReady;
				}

				// Upload de l'equirect HDR source (pixels RGBA float32, w*h*4
				// floats, alpha ignoré) dans un storage buffer GPU. À appeler
				// une fois ; les convolutions suivantes le réutilisent.
				bool UploadHDR(const float *rgbaPixels, uint32 width, uint32 height);

				// Convolution Lambert → cubemap irradiance. outFaces[f] doit
				// pointer sur irrSize*irrSize*4 octets (RGBA8), f = 0..5.
				// Sortie identique au chemin CPU (Reinhard + clamp inclus).
				bool ConvolveIrradiance(uint32 irrSize, uint8 *outFaces[6]);

				// Prefilter GGX d'UN niveau de mip (roughness donné, numSamples
				// échantillons). roughness < 1e-3 = mirror direct (mip 0).
				// outFaces[f] : mipSize*mipSize*4 octets RGBA8.
				bool PrefilterMip(uint32 mipSize, float32 roughness, uint32 numSamples, uint8 *outFaces[6]);

			private:
				struct Kernel {
						NkShaderHandle shader;
						NkPipelineHandle pipeline;
						NkDescSetHandle layout;
				};

				// Compile un kernel NkSL compute pour l'API du device (2 SSBO +
				// 1 UBO au binding 2). Renvoie false si compilation KO.
				bool CompileKernel(const char *name, const char *nkslSrc, Kernel &out);

				// Dispatch d'un kernel sur outSize²×6 threads (1D, groupes de 64),
				// readback du buffer de sortie (uints RGBA8 packés, face-major)
				// et copie dans les 6 faces de destination.
				bool RunKernel(const Kernel &kernel, uint32 outSize, float32 roughness, uint32 numSamples,
							   uint8 *outFaces[6]);

				NkIDevice *mDevice = nullptr;
				bool mReady = false;

				Kernel mIrradiance;
				Kernel mPrefilter;

				NkBufferHandle mHdrBuffer;	  // SSBO source (vec4[] RGBA float)
				NkBufferHandle mOutBuffer;	  // SSBO sortie (uint[] RGBA8 packé)
				NkBufferHandle mParamsBuffer; // UBO params (persistant)
				uint64 mOutCapacity = 0;	  // octets alloués de mOutBuffer
				uint32 mHdrWidth = 0;
				uint32 mHdrHeight = 0;
		};

	} // namespace renderer
} // namespace nkentseu
