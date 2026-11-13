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
			// (13/11) LE MILIEU PARTICIPANT (NkTcReglages.vol*) : trois vec4 a la suite de la scene -- (extinction,
			// albedo, phase, -), (boite min, -), (boite max, -) -- lus par cartes[5].w (0 : aucun, l'image d'avant)
			int32 offMilieu = 0;
			{
				NkTcPoserMilieu(r);
				float32 mn[3], mx[3], sag[3];
				if (NkTcMilieuBoite(mn, mx, sag)) {
					while (pq.scene.Size() % 16u != 0u)
						pq.scene.PushBack(0.f);
					offMilieu = (int32)(pq.scene.Size() / 4u);
					const float32 v[12] = {sag[0], sag[1], sag[2], 0.f, mn[0], mn[1], mn[2], 0.f, mx[0], mx[1], mx[2], 0.f};
					for (int32 k = 0; k < 12; ++k)
						pq.scene.PushBack(v[k]);
				}
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
			// (13/11) les decalages AU BIT PRES (NkTcBits, relus par floatBitsToInt) : au-dela de 2^24 vec4, un flottant les arrondissait
			Pose(pr.off0, NkTcBits(pq.offNoeuds), NkTcBits(pq.offTris), NkTcBits(pq.offSommets), NkTcBits(pq.offMats));
			Pose(pr.off1, NkTcBits(pq.offSources), (float32)pq.nSources, pq.puissanceTotale, NkTcBits(pq.offCiel));
			// cartes[12].w (libre) : (10/11) la regularisation contre les lucioles (NkTcReglages.regulariser) ;
			// cartes[13].w : (10/11) le decalage du vernis et des paillettes (0 : aucun, ou `sansVernis`)
			for (int32 c = 0; c < 16; ++c) {
				float32 w = 0.f;
				if (c == 12 && r.regulariser)
					w = 1.f;
				if (c == 13 && !r.sansVernis)
					w = NkTcBits(pq.offVernis);
				if (c == 14 && r.argile)
					w = 1.f; // (10/11) cartes[14].w : le mode argile (0 dans les executables d'avant)
				if (c == 15)
					w = NkTcBits(pq.offObjet); // (11/11) cartes[15].w : cartes d'objet, masque, feuille mince (0 : aucun)
				// (13/11) cartes[10].w : le rayon de la lentille (m ; 0 : le stenope d'avant) ; cartes[11].w : la
				// distance nette le long de l'axe (m) -- NkTcCamera.ouverture et miseAuPoint, comme Passe du processeur
				// (13/11) cartes[6].w : les candidates de l'echantillonnage des lampes (0 : 8, l'image d'avant)
				if (c == 5 && offMilieu > 0) // (13/11) cartes[5].w : le milieu participant (au bit pres)
					w = NkTcBits(offMilieu);
				if (c == 6 && r.risCandidats != 8)
					w = (float32)r.risCandidats;
				if (c == 10 && cam.ouverture > 0.f)
					w = (cam.focaleMM * 1e-3f) / (2.f * cam.ouverture);
				if (c == 11 && cam.ouverture > 0.f)
					w = cam.miseAuPoint > 0.f ? cam.miseAuPoint : fl;
				Pose(pr.cartes[c], (float32)pq.carteOff[c], (float32)pq.carteW[c], (float32)pq.carteH[c], w);
			}
			const float64 msPrep = (NkChrono::Now() - t0).ToMilliseconds();
			// LES PASSES : 1 echantillon par pixel et par bande de 200 lignes par envoi (un envoi
			// trop long declencherait la remise a zero du pilote, TDR ~2 s sous Windows)
			const NkElapsedTime t1 = NkChrono::Now();
			// (13/11) LES ENVOIS SOUS 0,5 s (le 13/11 a 05 h, 20 reinitialisations du pilote nvlddmkm 153 : un envoi qui
			// depasse ~2 s est tue par Windows, et l'image sort noire) : la bande part a 64 lignes, double tant que l'envoi
			// reste sous 40 ms (200 au plus), se divise par deux des qu'il depasse 150 ms (8 au moins ; la nuit aux 579 lampes : 382 ms avec 60 et 250) ; l'envoi le plus
			// long est ecrit dans le journal
			int32 bande = 64, bandeMin = 64, bandeMax = 64;
			float64 envoiMax = 0.0, envoiPremier = -1.0;
			int32 fait = 0;
			// (13/11) L'ECHANTILLONNAGE ADAPTATIF (NkTcReglages.adaptSeuil) : apres `adaptMin` passes, le plancher de
			// luminance (une fraction de la moyenne de l'image) est pose, le nuanceur saute les pixels convergés ;
			// toutes les 8 passes, on relit les sommes et on s'arrete quand moins de 0,2 % des pixels restent actifs
			float32 plancher = 0.f;
			NkVector<float32> s0, s3;
			int32 actifsFin = -1;
			// (13/11) un envoi d'ECHAUFFEMENT (une ligne, 0 echantillon : il n'ajoute que des zeros) : la mise en memoire des
			// tampons se paie sur un envoi qui ne calcule rien, pas sur une vraie bande
			{
				const NkElapsedTime tw = NkChrono::Now();
				Pose(pr.p0, 0.f, 0.f, (float32)r.rebonds, r.ecretage);
				Pose(pr.p1, 0.f, 1.f, mutation ? 1.f : 0.f, r.unRebond ? 1.f : 0.f);
				device->WriteBuffer(bUbo, &pr, sizeof(Params));
				NkICommandBuffer *cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
				cmd->Begin();
				cmd->BindComputePipeline(pipe);
				cmd->BindDescriptorSet(set, 0);
				cmd->Dispatch(((uint32)cam.L + 7u) / 8u, 1u, 1);
				cmd->UAVBarrier(bSortie);
				cmd->End();
				device->Submit(&cmd, 1);
				device->WaitIdle();
				device->DestroyCommandBuffer(cmd);
				NkPrintfSortie("[traceur-gpu] echauffement : %.1f ms\n", (NkChrono::Now() - tw).ToMilliseconds());
			}
			for (int32 s = 0; s < r.spp; ++s) {
				if (r.adaptSeuil > 0.f && plancher > 0.f) {
					pr.cartes[9][3] = r.adaptSeuil;
					pr.cartes[8][3] = (float32)r.adaptMin;
					pr.cartes[7][3] = plancher;
				}
				for (int32 y0 = 0; y0 < cam.H;) {
					const NkElapsedTime te0 = NkChrono::Now();
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
					y0 += bande;
					const float64 dt = (NkChrono::Now() - te0).ToMilliseconds();
					// le PREMIER envoi a part : il paie la mise en memoire des tampons (900 ms sur 305 Mo, mesure du 13/11)
					if (envoiPremier < 0.0)
						envoiPremier = dt;
					else
						envoiMax = dt > envoiMax ? dt : envoiMax;
					if (dt > 150.0 && bande > 8)
						bande = bande / 2 > 8 ? bande / 2 : 8;
					else if (dt < 40.0 && bande < 200)
						bande = bande * 2 < 200 ? bande * 2 : 200;
					bandeMin = bande < bandeMin ? bande : bandeMin;
					bandeMax = bande > bandeMax ? bande : bandeMax;
				}
				++fait;
				if (r.secondesMax > 0.f && (NkChrono::Now() - t1).ToMilliseconds() > (float64)r.secondesMax * 1000.0)
					break;
				if (r.adaptSeuil > 0.f && fait >= r.adaptMin && (fait - r.adaptMin) % 8 == 0) {
					s0.Resize(n * 4);
					s3.Resize(n * 4);
					device->ReadBuffer(bSortie, s0.Data(), (uint64)n * 16u, 0);
					device->ReadBuffer(bSortie, s3.Data(), (uint64)n * 16u, (uint64)n * 48u);
					auto lum = [](const float32 *c) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; };
					if (plancher <= 0.f) {
						float64 sm = 0.0;
						for (usize i = 0; i < n; ++i)
							sm += (float64)lum(&s0[i * 4]) / (float64)(s3[i * 4 + 2] > 0.f ? s3[i * 4 + 2] : 1.f);
						plancher = (float32)(sm / (float64)n) * r.adaptPlancherRel;
						plancher = plancher > 1e-6f ? plancher : 1e-6f;
					}
					int32 actifs = 0;
					for (usize i = 0; i < n; ++i) {
						const float32 nb = s3[i * 4 + 2];
						if (nb < (float32)r.adaptMin || nb <= 0.f) {
							++actifs;
							continue;
						}
						const float32 m = lum(&s0[i * 4]) / nb;
						const float32 vv = s3[i * 4 + 1] / nb - m * m;
						const float32 v = (vv > 0.f ? vv : 0.f) / nb;
						if (math::NkSqrt(v) > r.adaptSeuil * (m > plancher ? m : plancher))
							++actifs;
					}
					actifsFin = actifs;
					if ((float64)actifs < 0.002 * (float64)n)
						break;
				}
			}
			if (actifsFin >= 0)
				NkPrintfSortie("[traceur-gpu] ADAPTATIF : seuil %.3f, minimum %d, plancher %.5f ; %d passe(s), %d pixel(s) encore actifs "
							   "(%.2f %%)\n",
							   (double)r.adaptSeuil, (int)r.adaptMin, (double)plancher, (int)fait, (int)actifsFin,
							   100.0 * (float64)actifsFin / (float64)n);
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
			NkPrintfSortie("[traceur-gpu] ENVOI LE PLUS LONG : %.1f ms, le premier %.1f ms (bandes de %d a %d lignes ; borne visee "
						   "500 ms, le pilote coupe vers 2 s)%s\n",
						   envoiMax, envoiPremier, (int)bandeMin, (int)bandeMax,
						   (envoiMax > 500.0 || envoiPremier > 500.0) ? " -- AU-DELA DE 500 ms" : "");
			const bool ok = NkTcFinaliserSommes(cam, r, png, o4.Data(), fait, msRendu, 0, stats);
			device->FreeDescriptorSet(set);
			device->DestroyBuffer(bScene);
			device->DestroyBuffer(bCartes);
			device->DestroyBuffer(bSortie);
			device->DestroyBuffer(bUbo);
			device->DestroyPipeline(pipe);
			device->DestroyShader(sh);
			NkTcPoserMilieu(NkTcReglages{}); // (13/11) le milieu ne survit pas au rendu
			return ok;
		}

	} // namespace renderer
} // namespace nkentseu
