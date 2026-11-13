// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkBancAccel.cpp — (13/11/2026, plan RTX etapes 1 et 2) le banc des structures d'acceleration du RHI et de rayQuery
// dans NkSL (voir l'en-tete). Aucune fenetre : les dispositifs sont crees sans surface. Le seul travail sur la carte est
// une construction de quelques triangles et deux envois de 4 608 rayons (quelques millisecondes).
// =============================================================================
#include "NKRHI/Tools/NkBancAccel.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKSL/Compiler/NkGLSLCompiler.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

	namespace {
		// (etape 1) le nuanceur du banc ecrit en GLSL : chaque fil lance UN rayon dans la TLAS et rend (identifiant de
		// l'instance, distance)
		const char *kNuanceurBanc = "#version 460\n"
									"#extension GL_EXT_ray_query : require\n"
									"layout(local_size_x = 64) in;\n"
									"layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
									"layout(set = 0, binding = 1, std430) readonly buffer Rayons { vec4 r[]; };\n"
									"layout(set = 0, binding = 2, std430) buffer Sortie { vec4 o[]; };\n"
									"void main() {\n"
									"	uint i = gl_GlobalInvocationID.x;\n"
									"	if (i >= uint(o.length()))\n"
									"		return;\n"
									"	rayQueryEXT q;\n"
									"	rayQueryInitializeEXT(q, tlas, gl_RayFlagsOpaqueEXT, 0xFF, r[2u * i].xyz, 0.0, r[2u * i + 1u].xyz, 100.0);\n"
									"	while (rayQueryProceedEXT(q)) {\n"
									"	}\n"
									"	if (rayQueryGetIntersectionTypeEXT(q, true) == gl_RayQueryCommittedIntersectionTriangleEXT)\n"
									"		o[i] = vec4(float(rayQueryGetIntersectionInstanceCustomIndexEXT(q, true)), rayQueryGetIntersectionTEXT(q, true), 0.0, 1.0);\n"
									"	else\n"
									"		o[i] = vec4(-1.0, 0.0, 0.0, 1.0);\n"
									"}\n";

		// (etape 2) LE MEME PROGRAMME EN NkSL : types accelerationStructure et rayQuery, fonctions sans suffixe,
		// constantes RAY_* -- compile par NkSLCompiler (GLSL Vulkan puis SPIR-V 1.4)
		const char *kNuanceurNkSL = "@binding(set=0, binding=0) uniform accelerationStructure tlas;\n"
									"@binding(set=0, binding=1) buffer Rayons { vec4 r[]; } R;\n"
									"@binding(set=0, binding=2) buffer Sortie { vec4 o[]; } O;\n"
									"layout(local_size_x = 64) in;\n"
									"@stage(compute)\n"
									"@entry\n"
									"void main() {\n"
									"    uint i = gl_GlobalInvocationID.x;\n"
									"    if (i >= 4608u) return;\n"
									"    rayQuery q;\n"
									"    rayQueryInitialize(q, tlas, RAY_FLAG_OPAQUE, 255u, R.r[2u * i].xyz, 0.0, R.r[2u * i + 1u].xyz, 100.0);\n"
									"    while (rayQueryProceed(q)) {\n"
									"    }\n"
									"    if (rayQueryGetIntersectionType(q, true) == RAY_QUERY_COMMITTED_TRIANGLE)\n"
									"        O.o[i] = vec4(float(rayQueryGetIntersectionInstanceCustomIndex(q, true)), rayQueryGetIntersectionT(q, true), 0.0, 1.0);\n"
									"    else\n"
									"        O.o[i] = vec4(-1.0, 0.0, 0.0, 1.0);\n"
									"}\n";

		// le temoin : un nuanceur SANS rayQuery doit toujours se compiler partout (le refus vise rayQuery seul)
		const char *kNuanceurTemoin = "@binding(set=0, binding=0) buffer Sortie { vec4 o[]; } O;\n"
									  "layout(local_size_x = 64) in;\n"
									  "@stage(compute)\n"
									  "@entry\n"
									  "void main() {\n"
									  "    uint i = gl_GlobalInvocationID.x;\n"
									  "    O.o[i] = vec4(1.0, 0.0, 0.0, 1.0);\n"
									  "}\n";

		struct V3b {
				float64 x, y, z;
		};
		V3b Sous(V3b a, V3b b) {
			return {a.x - b.x, a.y - b.y, a.z - b.z};
		}
		V3b Vect(V3b a, V3b b) {
			return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
		}
		float64 Scal(V3b a, V3b b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}
		float64 Abs64(float64 v) {
			return v < 0.0 ? -v : v;
		}
		// Moller-Trumbore en double : t (ou -1), et `bord` vrai si le rayon passe a moins de 1e-4 d'une arete
		float64 Intersecter(V3b o, V3b d, V3b a, V3b b, V3b c, bool &bord) {
			const V3b e1 = Sous(b, a), e2 = Sous(c, a);
			const V3b p = Vect(d, e2);
			const float64 det = Scal(e1, p);
			if (Abs64(det) < 1e-12)
				return -1.0;
			const float64 inv = 1.0 / det;
			const V3b s = Sous(o, a);
			const float64 u = Scal(s, p) * inv;
			const V3b q = Vect(s, e1);
			const float64 v = Scal(d, q) * inv;
			const float64 t = Scal(e2, q) * inv;
			if (t <= 0.0)
				return -1.0;
			const float64 w = 1.0 - u - v;
			const float64 m = u < v ? (u < w ? u : w) : (v < w ? v : w);
			if (Abs64(m) < 1e-4)
				bord = true;
			return m >= 0.0 ? t : -1.0;
		}
		V3b Transformer(const float32 *M, const float32 *v) {
			return {(float64)M[0] * v[0] + (float64)M[1] * v[1] + (float64)M[2] * v[2] + (float64)M[3],
					(float64)M[4] * v[0] + (float64)M[5] * v[1] + (float64)M[6] * v[2] + (float64)M[7],
					(float64)M[8] * v[0] + (float64)M[9] * v[1] + (float64)M[10] * v[2] + (float64)M[11]};
		}
		// le mot d'indice k d'un SPIR-V rendu en octets (petit-boutiste)
		uint32 MotSpirv(const NkVector<uint8> &o, usize k) {
			if (k * 4u + 3u >= o.Size())
				return 0u;
			return (uint32)o[k * 4u] | ((uint32)o[k * 4u + 1u] << 8) | ((uint32)o[k * 4u + 2u] << 16) | ((uint32)o[k * 4u + 3u] << 24);
		}
	} // namespace

	int32 NkRhiBancAccel() {
		int32 echecs = 0;
		auto verdict = [&](bool ok, const char *quoi, float64 mesure, float64 attendu) {
			NkPrintfSortie("[rtx-banc] %-60s mesure %.6g attendu %.6g : %s\n", quoi, mesure, attendu, ok ? "VERT" : "ROUGE");
			if (!ok)
				++echecs;
		};

		// 0. (etape 2) rayQuery DANS NkSL, au processeur : le GLSL Vulkan, le SPIR-V, le refus des autres cibles
		NkVector<uint8> spirvNkSL;
		{
			NkSLCompiler slc;
			slc.EnableCache(false);
			const NkString src(kNuanceurNkSL);
			const NkSLCompileResult vk = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
			const bool okVk = vk.success && vk.source.Contains("#extension GL_EXT_ray_query : require") &&
							  vk.source.Contains("uniform accelerationStructureEXT tlas") && vk.source.Contains("rayQueryEXT q") &&
							  vk.source.Contains("rayQueryInitializeEXT(") && vk.source.Contains("gl_RayFlagsOpaqueEXT");
			if (!vk.success)
				for (uint32 k = 0; k < vk.errors.Size() && k < 4; ++k)
					NkPrintfSortie("[rtx-banc]   NkSL, ligne %u : %s\n", (unsigned)vk.errors[k].line, vk.errors[k].message.CStr());
			verdict(okVk, "NkSL -> GLSL Vulkan : extension, types et fonctions EXT", okVk ? 1.0 : 0.0, 1.0);
			const NkSLCompileResult sp = slc.Compile(src, NkSLStage::NK_COMPUTE, NkSLTarget::NK_SPIRV);
			bool capacite = false;
			const uint32 magique = MotSpirv(sp.bytecode, 0), version = MotSpirv(sp.bytecode, 1);
			if (sp.success && magique == 0x07230203u) {
				// OpCapability (code 17) RayQueryKHR (4472) dans le flux d'instructions
				usize k = 5;
				const usize n = sp.bytecode.Size() / 4u;
				while (k < n) {
					const uint32 w = MotSpirv(sp.bytecode, k);
					const uint32 nb = w >> 16, op = w & 0xFFFFu;
					if (op == 17u && nb >= 2u && MotSpirv(sp.bytecode, k + 1u) == 4472u)
						capacite = true;
					k += nb > 0u ? nb : 1u;
				}
				spirvNkSL = sp.bytecode;
			}
			if (!sp.success)
				for (uint32 k = 0; k < sp.errors.Size() && k < 4; ++k)
					NkPrintfSortie("[rtx-banc]   SPIR-V : %s\n", sp.errors[k].message.CStr());
			NkPrintfSortie("[rtx-banc] SPIR-V de NkSL : %u octets, magique 0x%08x, version 0x%08x, capacite RayQueryKHR %d\n",
						   (unsigned)sp.bytecode.Size(), (unsigned)magique, (unsigned)version, capacite ? 1 : 0);
			verdict(sp.success && capacite && version >= 0x00010400u, "NkSL -> SPIR-V : version 1.4 ou plus, capacite RayQueryKHR",
					(float64)((version >> 8) & 0xFFu) / 10.0 + (float64)((version >> 16) & 0xFFu), 1.4);
			// les autres cibles : un refus qui dit « non pris en charge »
			const NkSLTarget autres[7] = {NkSLTarget::NK_GLSL,	  NkSLTarget::NK_HLSL_DX11,		  NkSLTarget::NK_HLSL_DX12,
										  NkSLTarget::NK_MSL,	  NkSLTarget::NK_MSL_SPIRV_CROSS, NkSLTarget::NK_CPLUSPLUS,
										  NkSLTarget::NK_BYTECODE};
			int32 refus = 0;
			for (int32 t = 0; t < 7; ++t) {
				const NkSLCompileResult r = slc.Compile(src, NkSLStage::NK_COMPUTE, autres[t]);
				const bool clair = !r.success && !r.errors.Empty() && r.errors[0].message.Contains("non pris en charge");
				NkPrintfSortie("[rtx-banc]   %-16s : %s\n", NkSLTargetName(autres[t]),
							   clair ? r.errors[0].message.CStr() : (r.success ? "ACCEPTE (aucun refus)" : "refus SANS le message attendu"));
				if (clair)
					++refus;
			}
			verdict(refus == 7, "les 7 autres cibles refusent : « non pris en charge »", (float64)refus, 7.0);
			// le temoin sans rayQuery passe toujours (HLSL DX11 et GLSL)
			const NkSLCompileResult h = slc.Compile(NkString(kNuanceurTemoin), NkSLStage::NK_COMPUTE, NkSLTarget::NK_HLSL_DX11);
			const NkSLCompileResult g = slc.Compile(NkString(kNuanceurTemoin), NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
			verdict(h.success && g.success, "temoin sans rayQuery : HLSL DX11 et GLSL compilent", (h.success && g.success) ? 1.0 : 0.0, 1.0);
		}

		// la geometrie : un carre unite (deux triangles) dans le plan z = 0 ; trois instances
		const float32 sommets[12] = {-0.5f, -0.5f, 0.f, 0.5f, -0.5f, 0.f, 0.5f, 0.5f, 0.f, -0.5f, 0.5f, 0.f};
		const uint32 indices[6] = {0, 1, 2, 0, 2, 3};
		NkTlasInstance inst[3];
		{
			// 7 : translation seule ; 9 : rotation de 30 deg autour de z x echelle (1,5 ; 0,8) ; 3 : derriere 7, echelle 2
			const float32 a[12] = {1.f, 0.f, 0.f, -1.5f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, -5.f};
			const float32 b[12] = {1.2990381f, -0.4f, 0.f, 1.4f, 0.75f, 0.69282032f, 0.f, 0.2f, 0.f, 0.f, 1.f, -4.f};
			const float32 c[12] = {2.f, 0.f, 0.f, -1.2f, 0.f, 2.f, 0.f, 0.f, 0.f, 0.f, 1.f, -7.f};
			for (int32 k = 0; k < 12; ++k) {
				inst[0].transform[k] = a[k];
				inst[1].transform[k] = b[k];
				inst[2].transform[k] = c[k];
			}
			inst[0].instanceId = 7;
			inst[1].instanceId = 9;
			inst[2].instanceId = 3;
		}
		// les rayons : une grille 96 x 48 legerement penchee (aucun n'est aligne sur les axes)
		const int32 NX = 96, NY = 48, N = NX * NY;
		const float64 dl = Scal({0.07, -0.04, -1.0}, {0.07, -0.04, -1.0});
		const float64 nd = math::NkSqrt(dl);
		const V3b dir = {0.07 / nd, -0.04 / nd, -1.0 / nd};
		NkVector<float32> rayons;
		rayons.Resize((usize)N * 8u);
		NkVector<int32> idAttendu;
		idAttendu.Resize((usize)N);
		NkVector<float64> tAttendu;
		tAttendu.Resize((usize)N);
		NkVector<uint8> ambigu;
		ambigu.Resize((usize)N);
		int32 touches = 0, vus[3] = {0, 0, 0};
		for (int32 j = 0; j < NY; ++j)
			for (int32 i = 0; i < NX; ++i) {
				const int32 k = j * NX + i;
				const V3b o = {-3.5 + ((float64)i + 0.37) * (7.0 / NX), -1.5 + ((float64)j + 0.61) * (3.0 / NY), 1.0};
				const float32 r8[8] = {(float32)o.x, (float32)o.y, (float32)o.z, 0.f, (float32)dir.x, (float32)dir.y, (float32)dir.z, 0.f};
				for (int32 q = 0; q < 8; ++q)
					rayons[(usize)k * 8u + (usize)q] = r8[q];
				const V3b of = {(float64)r8[0], (float64)r8[1], (float64)r8[2]};
				const V3b df = {(float64)r8[4], (float64)r8[5], (float64)r8[6]};
				float64 tmin = 1e30;
				int32 id = -1;
				bool bord = false;
				for (int32 n = 0; n < 3; ++n)
					for (int32 t = 0; t < 2; ++t) {
						const V3b A = Transformer(inst[n].transform, sommets + indices[t * 3] * 3);
						const V3b B = Transformer(inst[n].transform, sommets + indices[t * 3 + 1] * 3);
						const V3b C = Transformer(inst[n].transform, sommets + indices[t * 3 + 2] * 3);
						const float64 tt = Intersecter(of, df, A, B, C, bord);
						if (tt > 0.0 && tt < tmin) {
							tmin = tt;
							id = (int32)inst[n].instanceId;
						}
					}
				idAttendu[(usize)k] = id;
				tAttendu[(usize)k] = tmin;
				ambigu[(usize)k] = bord ? 1u : 0u;
				if (id >= 0 && !bord) {
					++touches;
					vus[id == 7 ? 0 : (id == 9 ? 1 : 2)] += 1;
				}
			}

		// 1. le dispositif Vulkan qui DEMANDE les structures d'acceleration
		NkDeviceInitInfo init;
		init.api = NkGraphicsApi::NK_GFX_API_VULKAN;
		init.context.api = NkGraphicsApi::NK_GFX_API_VULKAN;
		init.context.vulkan.rayTracing = true;
		init.width = 64;
		init.height = 64;
		NkIDevice *dev = NkDeviceFactory::CreateForApi(NkGraphicsApi::NK_GFX_API_VULKAN, init);
		if (!dev || !dev->IsValid()) {
			verdict(false, "Vulkan sans fenetre, structures demandees : creation", 0.0, 1.0);
			if (dev)
				NkDeviceFactory::Destroy(dev);
			return echecs;
		}
		const bool rq = dev->GetCaps().rayQuery;
		verdict(rq, "Vulkan, structures demandees : caps.rayQuery", rq ? 1.0 : 0.0, 1.0);
		if (rq) {
			NkBlasDesc bd;
			bd.positions = sommets;
			bd.vertexCount = 4;
			bd.indices = indices;
			bd.indexCount = 6;
			bd.debugName = "banc_carre";
			NkAccelStructHandle blas = dev->CreateBottomLevelAS(bd);
			for (int32 n = 0; n < 3; ++n)
				inst[n].blas = blas;
			NkTlasDesc td;
			td.instances = inst;
			td.instanceCount = 3;
			td.debugName = "banc_scene";
			NkAccelStructHandle tlas = dev->CreateTopLevelAS(td);
			const uint64 tb = dev->GetAccelStructSize(blas), tt = dev->GetAccelStructSize(tlas);
			const uint64 ab = dev->GetAccelStructDeviceAddress(blas), at = dev->GetAccelStructDeviceAddress(tlas);
			verdict(blas.IsValid() && tlas.IsValid(), "BLAS et TLAS construites (poignees valides)", (blas.IsValid() && tlas.IsValid()) ? 1.0 : 0.0, 1.0);
			verdict(tb > 0 && tt > 0, "tailles > 0 (octets BLAS + TLAS)", (float64)(tb + tt), 1.0);
			verdict(ab != 0 && at != 0 && ab != at, "adresses sur la carte non nulles et distinctes", (ab != 0 && at != 0 && ab != at) ? 1.0 : 0.0, 1.0);

			// une passe : le SPIR-V donne, les 4 608 rayons, la comparaison au processeur
			auto tirer = [&](const NkVector<uint8> &octets, const char *nom) {
				NkShaderHandle sh;
				NkPipelineHandle pipe;
				NkDescSetHandle layout, set;
				NkBufferHandle bR, bO;
				if (!octets.Empty() && tlas.IsValid()) {
					NkShaderStageDesc st{};
					st.stage = NkShaderStage::NK_COMPUTE;
					st.spirvBinary = octets;
					NkShaderDesc sd;
					sd.AddStage(st);
					sh = dev->CreateShader(sd);
					NkDescriptorSetLayoutDesc ld;
					ld.Add(0, NkDescriptorType::NK_ACCELERATION_STRUCTURE, NkShaderStage::NK_COMPUTE);
					ld.Add(1, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
					ld.Add(2, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
					layout = dev->CreateDescriptorSetLayout(ld);
					NkComputePipelineDesc cpd;
					cpd.shader = sh;
					cpd.debugName = "banc_rtx";
					cpd.descriptorSetLayouts.PushBack(layout);
					pipe = dev->CreateComputePipeline(cpd);
					NkBufferDesc dr = NkBufferDesc::Storage((uint64)N * 32u, false);
					dr.initialData = rayons.Data();
					bR = dev->CreateBuffer(dr);
					NkVector<float32> zeros;
					zeros.Resize((usize)N * 4u);
					for (usize q = 0; q < zeros.Size(); ++q)
						zeros[q] = 0.f;
					NkBufferDesc dout = NkBufferDesc::Storage((uint64)N * 16u, false);
					dout.initialData = zeros.Data();
					bO = dev->CreateBuffer(dout);
					set = dev->AllocateDescriptorSet(layout);
					NkDescriptorWrite w[3]{};
					for (uint32 q = 0; q < 3; ++q) {
						w[q].set = set;
						w[q].binding = q;
					}
					w[0].type = NkDescriptorType::NK_ACCELERATION_STRUCTURE;
					w[0].accelStruct = tlas;
					w[1].type = NkDescriptorType::NK_STORAGE_BUFFER;
					w[1].buffer = bR;
					w[1].bufferRange = (uint64)N * 32u;
					w[2].type = NkDescriptorType::NK_STORAGE_BUFFER;
					w[2].buffer = bO;
					w[2].bufferRange = (uint64)N * 16u;
					dev->UpdateDescriptorSets(w, 3);
				}
				const bool pret = sh.IsValid() && pipe.IsValid() && bR.IsValid() && bO.IsValid() && set.IsValid();
				NkPrintfSortie("[rtx-banc] --- passe %s : pipeline %s\n", nom, pret ? "pret" : "REFUSE");
				verdict(pret, "pipeline de calcul rayQuery et tampons", pret ? 1.0 : 0.0, 1.0);
				if (pret) {
					NkICommandBuffer *cmd = dev->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
					cmd->Begin();
					cmd->BindComputePipeline(pipe);
					cmd->BindDescriptorSet(set, 0);
					cmd->Dispatch(((uint32)N + 63u) / 64u, 1u, 1u);
					cmd->UAVBarrier(bO);
					cmd->End();
					dev->Submit(&cmd, 1);
					dev->WaitIdle();
					dev->DestroyCommandBuffer(cmd);
					NkVector<float32> o4;
					o4.Resize((usize)N * 4u);
					dev->ReadBuffer(bO, o4.Data(), (uint64)N * 16u, 0);
					int32 faux = 0, compares = 0, ignores = 0;
					float64 ecartT = 0.0;
					for (int32 k = 0; k < N; ++k) {
						if (ambigu[(usize)k]) {
							++ignores;
							continue;
						}
						++compares;
						const int32 idg = (int32)(o4[(usize)k * 4u] + (o4[(usize)k * 4u] < 0.f ? -0.5f : 0.5f));
						if (idg != idAttendu[(usize)k]) {
							if (faux < 4)
								NkPrintfSortie("[rtx-banc]   rayon %d : instance %d sur la carte, %d au processeur\n", (int)k, idg,
											   (int)idAttendu[(usize)k]);
							++faux;
						} else if (idg >= 0) {
							const float64 e = Abs64((float64)o4[(usize)k * 4u + 1u] - tAttendu[(usize)k]);
							if (e > ecartT)
								ecartT = e;
						}
					}
					NkPrintfSortie("[rtx-banc] %d rayons : %d compares, %d ignores (a moins de 1e-4 d'une arete), %d touches (7 : %d, 9 : %d, 3 : %d)\n",
								   (int)N, (int)compares, (int)ignores, (int)touches, (int)vus[0], (int)vus[1], (int)vus[2]);
					verdict(faux == 0, "instance touchee : rayons en desaccord avec le processeur", (float64)faux, 0.0);
					verdict(ecartT < 1e-3, "distance : ecart max avec le processeur (m)", ecartT, 0.0);
					verdict(touches * 100 >= N * 15 && vus[0] > 0 && vus[1] > 0 && vus[2] > 0,
							"couverture : au moins 15 % de touches, les trois instances", (float64)touches / (float64)N, 0.15);
				}
				if (bR.IsValid())
					dev->DestroyBuffer(bR);
				if (bO.IsValid())
					dev->DestroyBuffer(bO);
				if (set.IsValid())
					dev->FreeDescriptorSet(set);
				if (pipe.IsValid())
					dev->DestroyPipeline(pipe);
				if (layout.IsValid())
					dev->DestroyDescriptorSetLayout(layout);
				if (sh.IsValid())
					dev->DestroyShader(sh);
			};
			// (etape 1) le GLSL ecrit a la main : GLSL -> SPIR-V 1.5 (client Vulkan 1.2)
			NkGLSLCompileResult spv = NkGLSLToSPIRV(NkShaderStage::NK_COMPUTE, kNuanceurBanc, "main", 105u);
			verdict(spv.success, "nuanceur GLSL rayQuery compile (SPIR-V 1.5)", spv.success ? (float64)spv.spirv.Size() : 0.0, 1.0);
			if (!spv.success && spv.errorLog)
				NkPrintfSortie("[rtx-banc] %s\n", spv.errorLog);
			NkVector<uint8> octetsGlsl;
			if (spv.success) {
				octetsGlsl.Resize(spv.spirv.Size() * 4u);
				const uint8 *mots = (const uint8 *)spv.spirv.Data();
				for (usize q = 0; q < octetsGlsl.Size(); ++q)
					octetsGlsl[q] = mots[q];
			}
			tirer(octetsGlsl, "GLSL ecrit a la main");
			// (etape 2) le meme programme ecrit en NkSL
			tirer(spirvNkSL, "NkSL (rayQuery, plan RTX etape 2)");
			dev->DestroyAccelStruct(tlas);
			dev->DestroyAccelStruct(blas);
			verdict(!tlas.IsValid() && !blas.IsValid() && dev->GetAccelStructSize(blas) == 0, "destruction (poignees remises a nul)", 1.0, 1.0);
		}
		NkDeviceFactory::Destroy(dev);

		// 2. « NON PRIS EN CHARGE » : Vulkan SANS la demande, puis le dorsal logiciel (le comportement par defaut de NkIDevice)
		auto nonPris = [&](NkGraphicsApi api, bool vulkan, const char *nom) {
			NkDeviceInitInfo ii;
			ii.api = api;
			ii.context.api = api;
			ii.width = 64;
			ii.height = 64;
			NkIDevice *d = NkDeviceFactory::CreateForApi(api, ii);
			if (!d || !d->IsValid()) {
				NkPrintfSortie("[rtx-banc] %s : dispositif non cree sans fenetre (verification sautee)\n", nom);
				if (d)
					NkDeviceFactory::Destroy(d);
				if (vulkan)
					verdict(false, "Vulkan sans la demande : creation", 0.0, 1.0);
				return;
			}
			NkBlasDesc bd;
			bd.positions = sommets;
			bd.vertexCount = 4;
			bd.indices = indices;
			bd.indexCount = 6;
			const NkAccelStructHandle h = d->CreateBottomLevelAS(bd);
			const bool ok = !d->GetCaps().rayQuery && !d->GetCaps().rayTracing && !h.IsValid() && d->GetAccelStructSize(h) == 0;
			char quoi[96];
			int32 p = 0;
			for (const char *s = nom; *s && p < 40; ++s)
				quoi[p++] = *s;
			const char *suite = " : non pris en charge (caps faux, poignee nulle)";
			for (const char *s = suite; *s && p < 95; ++s)
				quoi[p++] = *s;
			quoi[p] = 0;
			verdict(ok, quoi, ok ? 1.0 : 0.0, 1.0);
			NkDeviceFactory::Destroy(d);
		};
		nonPris(NkGraphicsApi::NK_GFX_API_VULKAN, true, "Vulkan sans la demande");
		nonPris(NkGraphicsApi::NK_GFX_API_SOFTWARE, false, "logiciel");
		return echecs;
	}

} // namespace nkentseu
