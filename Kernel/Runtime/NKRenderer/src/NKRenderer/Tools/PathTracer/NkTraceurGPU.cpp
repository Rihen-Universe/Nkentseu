// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkTraceurGPU.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Le traceur de chemins en nuanceur de calcul (voir l'en-tete). Le chemin de
// compilation par dorsal est celui de NkGpuAtomicWitness (NkSL -> GLSL Vulkan ->
// HLSL / SPIR-V / MSL ; GLSL OpenGL). Aucune bibliotheque standard.

#include "NKRenderer/Tools/PathTracer/NkTraceurGPU.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKCore/Text/NkSortie.h"
#include "NKFileSystem/NkFile.h"
#include "NKMath/NkFunctions.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// LE NUANCEUR : la meme physique que NkTraceurChemins.cpp, ligne pour ligne autant que
			// le langage le permet (Surf, EvalBsdf, PdfBsdf, TirerBsdf, Ciel, Directe, Chemin).
			// LE NUANCEUR vit dans Resources (comme tous les nuanceurs du moteur) : lu au lancement, depuis
			// la racine Nkentseu ou demarrent les applications.
			const char *kCheminNuanceur = "Resources/NKRenderer/Shaders/PathTracer/NkSL/traceur_chemins.comp.nksl";

			struct Params {
					float32 oeil[4], fw[4], rt[4], up[4];
					float32 p0[4], p1[4];
					float32 sol[4], solE[4], zen[4], hor[4];
					float32 drap[4];
					float32 off0[4], off1[4];
					float32 cartes[16][4]; // (10/11) 16 classes (tissu, velours, placage)
			};

			void Pose(float32 *v, float32 a, float32 b, float32 c, float32 d) {
				v[0] = a;
				v[1] = b;
				v[2] = c;
				v[3] = d;
			}

			// Compile le nuanceur pour le dorsal du device (le chemin de NkGpuAtomicWitness).
			::nkentseu::NkShaderHandle Compiler(NkIDevice *device) {
				const NkString src = NkFile::ReadAllText(kCheminNuanceur);
				if (src.Empty()) {
					NkPrintfSortie("[traceur-gpu] nuanceur introuvable : %s (lancer depuis la racine Nkentseu)\n", kCheminNuanceur);
					return ::nkentseu::NkShaderHandle{};
				}
				NkSLCompiler slc;
				NkSLCompileResult gl = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
				if (!gl.success) {
					for (uint32 i = 0; i < gl.errors.Size() && i < 5; ++i)
						NkPrintfSortie("[traceur-gpu] NkSL, ligne %u : %s\n", (unsigned)gl.errors[i].line, gl.errors[i].message.CStr());
					return ::nkentseu::NkShaderHandle{};
				}
				NkShaderDesc sd;
				sd.debugName = "traceur_chemins";
				NkShaderConvertResult hl, sp, ms;
				NkSLCompileResult glo;
				const NkGraphicsApi api = device->GetApi();
				if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
					hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, 50u, "traceur_chemins");
					if (!hl.success) {
						NkPrintfSortie("[traceur-gpu] GLSL -> HLSL refuse : %s\n", hl.errors.CStr());
						return ::nkentseu::NkShaderHandle{};
					}
					sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
				} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
					sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, "traceur_chemins");
					if (!sp.success)
						return ::nkentseu::NkShaderHandle{};
					sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
				} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
					ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, "traceur_chemins");
					if (!ms.success)
						return ::nkentseu::NkShaderHandle{};
					sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
				} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
					glo = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
					if (!glo.success)
						return ::nkentseu::NkShaderHandle{};
					sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
				} else
					return ::nkentseu::NkShaderHandle{};
				return device->CreateShader(sd);
			}
		} // namespace

		bool NkTcRendreGPU(NkIDevice *device, const NkTcCamera &cam, const NkTcReglages &r, const char *png, NkTcStats *stats,
						   bool mutation) {
			if (!device || !device->GetCaps().computeShaders) {
				NkPrintfSortie("[traceur-gpu] pas de nuanceur de calcul sur ce device : rendu GPU impossible\n");
				return false;
			}
			const NkElapsedTime t0 = NkChrono::Now();
			NkTcPaquet pq;
			if (!NkTcEmpaqueter(pq)) {
				NkPrintfSortie("[traceur-gpu] scene non preparee\n");
				return false;
			}
			::nkentseu::NkShaderHandle sh = Compiler(device);
			if (!sh.IsValid()) {
				NkPrintfSortie("[traceur-gpu] nuanceur refuse\n");
				return false;
			}
			NkDescriptorSetLayoutDesc ld;
			ld.Add(0, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(1, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(2, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
			ld.Add(3, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
			NkDescSetHandle layout = device->CreateDescriptorSetLayout(ld);
			NkComputePipelineDesc cpd;
			cpd.shader = sh;
			cpd.debugName = "traceur_chemins";
			cpd.descriptorSetLayouts.PushBack(layout);
			NkPipelineHandle pipe = device->CreateComputePipeline(cpd);
			if (!pipe.IsValid()) {
				NkPrintfSortie("[traceur-gpu] pipeline de calcul refuse\n");
				return false;
			}
			const usize n = (usize)cam.L * (usize)cam.H;
			NkBufferDesc bs = NkBufferDesc::Storage((uint64)pq.scene.Size() * 4u, false);
			bs.initialData = pq.scene.Data();
			bs.debugName = "traceur_scene";
			NkBufferHandle bScene = device->CreateBuffer(bs);
			NkBufferDesc bc = NkBufferDesc::Storage((uint64)pq.cartes.Size() * 4u, false);
			bc.initialData = pq.cartes.Data();
			bc.debugName = "traceur_cartes";
			NkBufferHandle bCartes = device->CreateBuffer(bc);
			NkVector<float32> zeros;
			zeros.Resize(n * 16);
			for (usize i = 0; i < zeros.Size(); ++i)
				zeros[i] = 0.f;
			// PAS « lisible par le processeur » : sous DX11 un tampon de relecture est un tampon de
			// TRANSIT, qu'aucune vue UAV ne peut lier -- le nuanceur ecrivait dans le vide, l'image
			// sortait noire (mesure du 10/11 : 0,0 partout). ReadBuffer fait sa propre copie de transit.
			NkBufferDesc bo = NkBufferDesc::Storage((uint64)n * 64u, false);
			bo.initialData = zeros.Data();
			bo.debugName = "traceur_sortie";
			NkBufferHandle bSortie = device->CreateBuffer(bo);
			NkBufferHandle bUbo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(Params)));
			if (!bScene.IsValid() || !bCartes.IsValid() || !bSortie.IsValid() || !bUbo.IsValid()) {
				NkPrintfSortie("[traceur-gpu] tampons refuses\n");
				return false;
			}
			NkDescSetHandle set = device->AllocateDescriptorSet(layout);
			{
				NkDescriptorWrite w[3]{};
				const NkBufferHandle b[3] = {bScene, bCartes, bSortie};
				for (uint32 i = 0; i < 3; ++i) {
					w[i].set = set;
					w[i].binding = i;
					w[i].type = NkDescriptorType::NK_STORAGE_BUFFER;
					w[i].buffer = b[i];
				}
				device->UpdateDescriptorSets(w, 3);
			}
			device->BindUniformBuffer(set, 3, bUbo);
			// les parametres fixes
			Params pr{};
			const float32 fw0[3] = {cam.cible[0] - cam.oeil[0], cam.cible[1] - cam.oeil[1], cam.cible[2] - cam.oeil[2]};
			const float32 fl = math::NkSqrt(fw0[0] * fw0[0] + fw0[1] * fw0[1] + fw0[2] * fw0[2]);
			const float32 fw[3] = {fw0[0] / fl, fw0[1] / fl, fw0[2] / fl};
			float32 rt[3] = {-fw[2], 0.f, fw[0]}; // cross(fw, (0,1,0))
			const float32 rl = math::NkSqrt(rt[0] * rt[0] + rt[2] * rt[2]);
			rt[0] /= (rl > 1e-12f ? rl : 1e-12f);
			rt[2] /= (rl > 1e-12f ? rl : 1e-12f);
			const float32 up[3] = {rt[1] * fw[2] - rt[2] * fw[1], rt[2] * fw[0] - rt[0] * fw[2], rt[0] * fw[1] - rt[1] * fw[0]};
			const float32 th = 18.f / (cam.focaleMM > 1.f ? cam.focaleMM : 1.f);
			Pose(pr.oeil, cam.oeil[0], cam.oeil[1], cam.oeil[2], th);
			Pose(pr.fw, fw[0], fw[1], fw[2], (float32)cam.L / (float32)cam.H);
			Pose(pr.rt, rt[0], rt[1], rt[2], (float32)cam.L);
			Pose(pr.up, up[0], up[1], up[2], (float32)cam.H);
			Pose(pr.sol, pq.soleilDir[0], pq.soleilDir[1], pq.soleilDir[2], 0.99998918f);
			// (10/11/2026) le ciel en table : solE.w = hauteur, hor.w = largeur, off1.w = decalage (0 : degrade)
			Pose(pr.solE, pq.soleilE[0], pq.soleilE[1], pq.soleilE[2], (float32)pq.cielH);
			Pose(pr.zen, pq.zenith[0], pq.zenith[1], pq.zenith[2], pq.cielIntensite);
			Pose(pr.hor, pq.horizon[0], pq.horizon[1], pq.horizon[2], (float32)pq.cielW);
			Pose(pr.drap, r.sansSoleil ? 1.f : 0.f, r.sansCiel ? 1.f : 0.f, r.sansSources ? 1.f : 0.f, r.sansCartes ? 1.f : 0.f);
			Pose(pr.off0, (float32)pq.offNoeuds, (float32)pq.offTris, (float32)pq.offSommets, (float32)pq.offMats);
			Pose(pr.off1, (float32)pq.offSources, (float32)pq.nSources, pq.puissanceTotale, (float32)pq.offCiel);
			// cartes[12].w (libre) : (10/11) la regularisation contre les lucioles (NkTcReglages.regulariser)
			for (int32 c = 0; c < 16; ++c)
				Pose(pr.cartes[c], (float32)pq.carteOff[c], (float32)pq.carteW[c], (float32)pq.carteH[c],
					 c == 12 && r.regulariser ? 1.f : 0.f);
			const float64 msPrep = (NkChrono::Now() - t0).ToMilliseconds();
			// LES PASSES : 1 echantillon par pixel et par bande de 200 lignes par envoi (un envoi
			// trop long declencherait la remise a zero du pilote, TDR ~2 s sous Windows)
			const NkElapsedTime t1 = NkChrono::Now();
			const int32 bande = 200;
			int32 fait = 0;
			for (int32 s = 0; s < r.spp; ++s) {
				for (int32 y0 = 0; y0 < cam.H; y0 += bande) {
					Pose(pr.p0, 1.f, (float32)s, (float32)r.rebonds, r.ecretage);
					Pose(pr.p1, (float32)y0, (float32)bande, mutation ? 1.f : 0.f, r.unRebond ? 1.f : 0.f);
					device->WriteBuffer(bUbo, &pr, sizeof(Params));
					NkICommandBuffer *cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
					cmd->Begin();
					cmd->BindComputePipeline(pipe);
					cmd->BindDescriptorSet(set, 0);
					cmd->Dispatch(((uint32)cam.L + 7u) / 8u, ((uint32)bande + 7u) / 8u, 1);
					cmd->UAVBarrier(bSortie);
					cmd->End();
					device->Submit(&cmd, 1);
					device->WaitIdle();
					device->DestroyCommandBuffer(cmd);
				}
				++fait;
				if (r.secondesMax > 0.f && (NkChrono::Now() - t1).ToMilliseconds() > (float64)r.secondesMax * 1000.0)
					break;
			}
			const float64 msRendu = (NkChrono::Now() - t1).ToMilliseconds();
			NkVector<float32> o4;
			o4.Resize(n * 16);
			device->ReadBuffer(bSortie, o4.Data(), (uint64)n * 64u, 0);
			NkPrintfSortie("[traceur-gpu] %dx%d, %d ech./pixel : preparation %.0f ms (compilation, tampons %.1f + %.1f Mo), rendu %.2f s "
						   "(%.1f Mech./s)%s\n",
						   (int)cam.L, (int)cam.H, (int)fait, msPrep, (float64)pq.scene.Size() * 4.0 / 1048576.0,
						   (float64)pq.cartes.Size() * 4.0 / 1048576.0, msRendu / 1000.0,
						   msRendu > 0.0 ? (float64)n * (float64)fait / msRendu / 1000.0 : 0.0,
						   mutation ? " -- MUTATION (rayons d'ombre coupes : pas d'ombre)" : "");
			const bool ok = NkTcFinaliserSommes(cam, r, png, o4.Data(), fait, msRendu, 0, stats);
			device->FreeDescriptorSet(set);
			device->DestroyBuffer(bScene);
			device->DestroyBuffer(bCartes);
			device->DestroyBuffer(bSortie);
			device->DestroyBuffer(bUbo);
			device->DestroyPipeline(pipe);
			device->DestroyShader(sh);
			return ok;
		}

	} // namespace renderer
} // namespace nkentseu
