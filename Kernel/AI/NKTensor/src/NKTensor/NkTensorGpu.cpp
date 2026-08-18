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
#include "NKTime/NkChrono.h" // profil par noyau : horloge monotone haute resolution
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace ai {
		void NkGpuSignalerDefaut(const char *ou, const char *quoi, int64 valeur);


		// ---- État interne (pimpl) : tout NKRHI/NKSL confiné ici -----------------
		struct NkTensorGpu::Impl {
				NkIDevice *device = nullptr;
				bool tried = false;
				const char *backend = "none";

				NkUnorderedMap<uint64, NkBufferHandle> buffers; // id opaque -> handle
				NkUnorderedMap<uint64, nk_size> tailles;	   // id opaque -> octets (suivi VRAM)
				uint64 nextId = 1;

				// RESERVE DE TAMPONS (chantier n°2) : cle = TAILLE EXACTE en octets.
				// Jamais « assez grand » : rendre un tampon plus large que demande
				// marcherait a l'usage et fausserait tout calcul de trafic ou de borne.
				NkUnorderedMap<uint64, NkVector<NkBufferHandle>> reserve;

				struct Kernel {
						NkString name;
						NkShaderHandle shader;
						NkPipelineHandle pipe;
						NkDescSetHandle layout;
						NkBufferHandle params; // UBO 16o persistant (pas de churn par dispatch)
						// Vrai des que le controle « sortie non nulle » a ete passe une fois.
						bool verifie = false;
				};

				NkVector<Kernel> kernels; // cache par nom (peu d'entrées -> linéaire)

				// Compile (ou récupère du cache) un kernel NkSL compute.
				// nBuffers storage buffers (bindings 0..n-1) + 1 UBO au binding uboBinding.
				// ⚠ UN NOYAU QUI NE COMPILE PAS DOIT SE VOIR.
				// Ces echecs n'etaient ecrits QUE dans le journal, que personne ne lit
				// pendant un entrainement. Resultat constate le 13 aout 2026 : le noyau
				// RoPE ne compilait pas (`half`, mot reserve GLSL), le tampon de sortie
				// restait a ZERO, et l'entrainement continuait sans une alerte. Chaque
				// sortie en echec incremente desormais le compteur de defauts, que
				// l'entrainement consulte (NkTensorGpu::DefautCount).
				Kernel *GetOrCompile(const char *name, const NkString &nksl, uint32 nBuffers, uint32 uboBinding) {
					for (uint32 i = 0; i < kernels.Size(); i++)
						if (kernels[i].name == name)
							return &kernels[i];

					NkSLCompiler slc;
					NkSLCompileResult gl = slc.Compile(nksl, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
					if (!gl.success) {
						logger_src.Errorf("[NkTensorGpu] NkSL->GLSL KO (%s)\n", name);
						NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : NkSL vers GLSL refuse", 0);
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
							NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : GLSL vers HLSL refuse", 0);
							return nullptr;
						}
						sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
					} else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
						sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, name);
						if (!sp.success) {
							logger_src.Errorf("[NkTensorGpu] GLSL->SPIRV KO (%s)\n", name);
							NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : GLSL vers SPIRV refuse", 0);
							return nullptr;
						}
						sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
					} else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
						ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, name);
						if (!ms.success) {
							logger_src.Errorf("[NkTensorGpu] GLSL->MSL KO (%s)\n", name);
							NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : GLSL vers MSL refuse", 0);
							return nullptr;
						}
						sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
					} else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
						glo = slc.Compile(nksl, NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL);
						if (!glo.success) {
							logger_src.Errorf("[NkTensorGpu] NkSL->GLSL(GL) KO (%s)\n", name);
							NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : NkSL vers GLSL(GL) refuse", 0);
							return nullptr;
						}
						sd.AddGLSL(NkShaderStage::NK_COMPUTE, glo.source.CStr(), "main");
					} else {
						logger_src.Errorf("[NkTensorGpu] API compute non supportée (%s)\n", name);
						NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : API compute non supportee", 0);
						return nullptr;
					}

					NkShaderHandle sh = device->CreateShader(sd);
					if (!sh.IsValid()) {
						logger_src.Errorf("[NkTensorGpu] CreateShader KO (%s)\n", name);
						NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : CreateShader refuse", 0);
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
						NkGpuSignalerDefaut(name, "COMPILATION DU NOYAU : CreateComputePipeline refuse", 0);
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
					// Ces deux limites décident de la taille de lot maximale utilisable.
					// Les journaliser au démarrage évite d'avoir à les deviner quand un
					// entraînement cesse d'apprendre sans rien dire.
					{
						const NkDeviceCaps &c = dev->GetCaps();
						logger_src.Infof("[NkTensorGpu] limites : tampon adressable par shader = %llu Mo, "
										 "memoire video = %llu Mo\n",
										 (unsigned long long)(c.maxStorageBufferRange / (1024ull * 1024ull)),
										 (unsigned long long)(c.vramBytes / (1024ull * 1024ull)));
					}
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
				// La reserve d'abord : ses tampons sont vivants et le peripherique va
				// disparaitre. Les oublier ici fuirait a l'arret sans le moindre
				// message — le genre de defaut qu'aucun test ne voit.
				ReserveVider();
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
		// Nombre total d'operations GPU lancees. Chaque operation elementaire paie
		// aujourd'hui un cout FIXE (allocation d'un jeu de descripteurs, creation
		// d'un tampon de commandes, soumission, attente d'inactivite complete du
		// peripherique, puis destruction). Diviser le temps d'un pas par ce nombre
		// donne ce cout fixe — c'est lui qui decide s'il vaut la peine d'etre
		// attaque, plutot qu'une intuition.
		static int64 gGpuOps = 0;

		// Occupation VRAM SUIVIE PAR NOUS : somme des tampons vivants, et son PIC.
		// Le pic est la seule grandeur qui decide si une configuration tient : une
		// moyenne, ou un releve a un instant arbitraire, rate le moment ou la passe
		// ARRIERE materialise les gradients par-dessus les activations.
		// ⚠️ Ne compte QUE nos tampons de calcul : ni le pilote, ni la fragmentation,
		// ni les allocations d'autres modules. C'est un PLANCHER de l'occupation
		// reelle, pas son total — d'ou la marge exigee dans le critere.
		//
		// ⚠️ DEUX PICS, PAS UN (corrige 2026-08-17). Un tampon RETENU par la reserve
		// n'est plus « vivant » pour le calcul mais sa VRAM reste allouee : le
		// compter comme libere faisait afficher un pic IDENTIQUE avec et sans
		// reserve — un instrument incapable de dire la seule chose qu'on lui
		// demandera le jour ou on discutera d'agrandir le budget.
		//   gVramPic       : pic de l'occupation PHYSIQUE (vivants + retenus) —
		//                    la grandeur qui decide si ca tient sur la carte ;
		//   gVramPicCalcul : pic des tampons de calcul SEULS — le besoin
		//                    incompressible, independant de la politique de cache.
		// Melanger les deux dans un seul chiffre serait la lecon des metriques
		// figees de NKXR : une ligne qui melange du vivant et du mort sans le dire.
		static int64 gVramVivante = 0;
		static int64 gVramPic = 0;		 // pic PHYSIQUE : vivants + retenus par la reserve
		static int64 gVramPicCalcul = 0; // pic des tampons de calcul seuls

		// ---- RESERVE DE TAMPONS : COMPTEURS TEMOINS -----------------------------
		// ⚠️ Ces compteurs ne sont PAS de la decoration : une reserve REPOND
		// TOUJOURS. Sans eux, « la reserve marche » est indiscernable de « la
		// reserve ne sert jamais et tout retombe en allocation ». La seule preuve
		// qu'elle sert est `gReserveServis > 0`, et la seule preuve que
		// l'instrument est juste est `servis + neufs == appels a CreateBuffer`.
		static bool gReserveActive = false;			   // defaut LEGACY : on n'active rien en douce
		static int64 gReserveBudget = 512ll * 1024 * 1024; // 512 Mo retenus au plus
		static int64 gReserveServis = 0;			   // rendus PAR la reserve
		static int64 gReserveNeufs = 0;				   // allocations REELLES
		static int64 gReserveOctets = 0;			   // VRAM immobilisee par la reserve
		static int64 gReserveTampons = 0;			   // nombre de tampons retenus
		static int64 gReserveEvictions = 0;			   // detruits faute de budget

		// ---- PROFIL PAR NOYAU ---------------------------------------------------
		// Voir NkTensorGpu.h pour ce que cette table mesure — et surtout ce qu'elle
		// ne mesure PAS (temps mural attribue, pas temps GPU pur).
		struct NkGpuProfilLigne {
				char nom[40];
				int64 appels;
				double ns;
				double flops;  // operations flottantes utiles demandees
				double octets; // trafic OBLIGATOIRE (lectures + ecritures minimales)
		};

		static NkGpuProfilLigne gProfil[64];
		static uint32 gProfilN = 0;
		static bool gProfilActif = false;

		// ---- ATTRIBUTION DES BASCULES CPU -> GPU --------------------------------
		// Le profil du 15/08 comptait 2 704 `~upload` par pas, ~12,6 Go, et
		// `~download` a ~0 : ce ne sont donc PAS des allers-retours, ce sont des
		// tenseurs NES sur CPU puis montes. On ne peut pas supprimer des uploads
		// dont on ignore l'origine — d'ou cette table.
		//
		// ⚠️ TOUS passent par UN SEUL site : `NkTensor::ToGPU()`. Ce qui varie est
		// son APPELANT, et c'est lui qu'il faut nommer. On enregistre donc
		// l'adresse de retour, resolue apres coup par `addr2line` : aucun
		// etiquetage a poser sur les 68 gardes `(t.Device()==GPU) ? t : t.ToGPU()`,
		// donc aucun risque d'en etiqueter 67 et de conclure sur les 68.
		//
		// ⚠️ ASLR : une adresse d'execution ne veut rien dire seule. On journalise
		// donc aussi l'adresse d'une ANCRE du meme module ; l'ecart entre les deux
		// est stable, et `nm` donne l'adresse de liaison de l'ancre.
		struct NkGpuBasculeLigne {
				const void *retour;
				int64 appels;
				double octets;
		};

		static NkGpuBasculeLigne gBascule[64];
		static uint32 gBasculeN = 0;
		static int64 gBasculeHorsTable = 0;

		void NkGpuAncre() {} // ancre de resolution — ne fait rien, doit exister

		static void NkGpuAttribuerBascule(const void *retour, double octets) {
			if (!gProfilActif)
				return;
			for (uint32 i = 0; i < gBasculeN; ++i) {
				if (gBascule[i].retour == retour) {
					gBascule[i].appels += 1;
					gBascule[i].octets += octets;
					return;
				}
			}
			if (gBasculeN >= 64) {
				++gBasculeHorsTable; // on perd le site, jamais le compte
				return;
			}
			NkGpuBasculeLigne &l = gBascule[gBasculeN++];
			l.retour = retour;
			l.appels = 1;
			l.octets = octets;
		}

		static NkGpuProfilLigne *NkGpuProfilLigneDe(const char *nom) {
			for (uint32 i = 0; i < gProfilN; ++i) {
				const char *a = gProfil[i].nom;
				const char *b = nom;
				while (*a && *a == *b) {
					++a;
					++b;
				}
				if (*a == 0 && *b == 0)
					return &gProfil[i];
			}
			if (gProfilN >= 64)
				return nullptr; // table pleine : on perd la ligne, jamais la mesure des autres
			NkGpuProfilLigne &l = gProfil[gProfilN++];
			uint32 i = 0;
			for (; nom[i] && i < 39; ++i)
				l.nom[i] = nom[i];
			l.nom[i] = 0;
			l.appels = 0;
			l.ns = 0.0;
			l.flops = 0.0;
			l.octets = 0.0;
			return &l;
		}

		// Chronometre de portee. Le cout par appel est d'une lecture d'horloge haute
		// resolution (~20 ns) contre un dispatch suivi d'un WaitIdle (~100 µs mesure) :
		// l'instrument ne peut pas deplacer ce qu'il mesure.
		class NkGpuChrono {
			public:
				explicit NkGpuChrono(const char *nom) : mNom(nom) {
					if (gProfilActif)
						mT0 = NkChrono::Now().nanoseconds;
				}

				// Travail demande par l'operation : sert a comparer chaque noyau a son
				// PROPRE plafond (bande passante x intensite) et non a la crete de la carte.
				void Travail(double flops, double octets) {
					mFlops = flops;
					mOctets = octets;
				}

				~NkGpuChrono() {
					if (!gProfilActif)
						return;
					NkGpuProfilLigne *l = NkGpuProfilLigneDe(mNom);
					if (!l)
						return;
					l->appels += 1;
					l->ns += NkChrono::Now().nanoseconds - mT0;
					l->flops += mFlops;
					l->octets += mOctets;
				}

			private:
				const char *mNom;
				double mT0 = 0.0;
				double mFlops = 0.0;
				double mOctets = 0.0;
		};

		void NkTensorGpu::ProfilRaz(bool actif) {
			gProfilN = 0;
			gBasculeN = 0;
			gBasculeHorsTable = 0;
			gProfilActif = actif;
		}

		void NkTensorGpu::ProfilRapport(double secondesMurales, int64 pas) {
			if (gProfilN == 0) {
				logger.Info("[profil noyaux] AUCUNE ligne — le profil n'a pas ete arme (ProfilRaz(true)) ou "
							"aucune operation GPU n'a eu lieu dans la fenetre.");
				return;
			}
			double totalNs = 0.0;
			int64 totalAppels = 0;
			for (uint32 i = 0; i < gProfilN; ++i) {
				totalNs += gProfil[i].ns;
				totalAppels += gProfil[i].appels;
			}
			// Tri decroissant par temps (insertion : au plus 64 lignes).
			uint32 ordre[64];
			for (uint32 i = 0; i < gProfilN; ++i)
				ordre[i] = i;
			for (uint32 i = 1; i < gProfilN; ++i) {
				uint32 v = ordre[i];
				uint32 j = i;
				while (j > 0 && gProfil[ordre[j - 1]].ns < gProfil[v].ns) {
					ordre[j] = ordre[j - 1];
					--j;
				}
				ordre[j] = v;
			}

			logger.Info("=== PROFIL PAR NOYAU — fenetre de {0} pas, {1} s murales, {2} operations ===",
						(long long)pas, secondesMurales, (long long)totalAppels);
			logger.Info("    (temps MURAL attribue par operation : noyau + cout fixe de lancement, "
						"chaque dispatch etant suivi d'un WaitIdle. Ce n'est pas un temps GPU pur.)");
			logger.Info("    {0:<22} {1:>9} {2:>11} {3:>7} {4:>9} {5:>10} {6:>8} {7:>9}", "nom", "appels", "ms",
						"% mural", "µs/appel", "GFLOP/s", "Go/s", "FLOP/o");
			for (uint32 k = 0; k < gProfilN; ++k) {
				const NkGpuProfilLigne &l = gProfil[ordre[k]];
				const double ms = l.ns / 1.0e6;
				const double sec = l.ns / 1.0e9;
				logger.Info("    {0:<22} {1:>9} {2:>11.3f} {3:>6.2f}% {4:>9.1f} {5:>10.1f} {6:>8.1f} {7:>9.3f}",
							l.nom, (long long)l.appels, ms,
							(secondesMurales > 0.0) ? (sec / secondesMurales * 100.0) : 0.0,
							(l.appels > 0) ? (l.ns / 1000.0 / (double)l.appels) : 0.0,
							(sec > 0.0) ? (l.flops / sec / 1.0e9) : 0.0, (sec > 0.0) ? (l.octets / sec / 1.0e9) : 0.0,
							(l.octets > 0.0) ? (l.flops / l.octets) : 0.0);
			}
			logger.Info("    TOTAL instrumente : {0} ms, soit {1}% du temps mural de la fenetre. Le reste "
						"est HORS instrumentation (construction des lots, autograd CPU, journalisation).",
						totalNs / 1.0e6, (secondesMurales > 0.0) ? (totalNs / 1.0e9 / secondesMurales * 100.0) : 0.0);
			// ⚠️ DEUX DEFAUTS CORRIGES ICI LE 2026-08-18, et le second est le plus couteux.
			//
			// 1. Le chiffre etait celui d'une RTX 3070 de BUREAU (20 300 GFLOP/s). La
			//    machine est une RTX 3070 **Laptop**, ~16 600 GFLOP/s. Signale par Q12
			//    le 2026-08-16 ; la ligne n'avait pas bouge. Tout « % de crete » imprime
			//    entre-temps est surestime d'environ 22 %.
			//
			// 2. La colonne `FLOP/o` ci-dessus n'est PAS l'intensite du noyau : elle est
			//    calculee sur les octets LOGIQUES (chaque matrice comptee une fois,
			//    caches parfaits), pas sur le trafic DRAM reel. Pour `matmul_t4` sur la
			//    tete de sortie elle affiche ~223 FLOP/o, alors que le trafic reel du
			//    noyau pave 4x4 vaut 2·M·N·K octets, soit une intensite de **1,0**.
			//    Comparer la colonne au rapport ci-dessous fait donc conclure « borne
			//    calcul » sur un noyau qui ne l'est peut-etre pas — le piege a mordu.
			//    La colonne est un PLANCHER de trafic, donc un PLAFOND d'intensite.
			logger.Info("    Repere machine : une RTX 3070 Laptop fait ~16 600 GFLOP/s pour ~448 Go/s, "
						"soit ~37 FLOP/octet. ⚠️ La colonne FLOP/o est une intensite LOGIQUE (trafic "
						"minimal, caches parfaits) : c'est un MAJORANT. Ne pas la comparer telle quelle "
						"a ce rapport — il faut l'intensite du trafic REEL du noyau, qui est plus basse.");

			// ---- Attribution des bascules CPU -> GPU ----------------------------
			if (gBasculeN > 0) {
				uint32 ord[64];
				for (uint32 i = 0; i < gBasculeN; ++i)
					ord[i] = i;
				for (uint32 i = 1; i < gBasculeN; ++i) {
					uint32 v = ord[i];
					uint32 j = i;
					while (j > 0 && gBascule[ord[j - 1]].appels < gBascule[v].appels) {
						ord[j] = ord[j - 1];
						--j;
					}
					ord[j] = v;
				}
				int64 totAppels = 0;
				double totOctets = 0.0;
				for (uint32 i = 0; i < gBasculeN; ++i) {
					totAppels += gBascule[i].appels;
					totOctets += gBascule[i].octets;
				}
				logger.Info("=== BASCULES CPU -> GPU (ToGPU) — {0} sites, {1} appels, {2} Mo, sur {3} pas ===",
							(long long)gBasculeN, (long long)totAppels, totOctets / 1.0e6, (long long)pas);
				logger.Info("    ⚠️ CHAQUE LIGNE EST UNE ALLOCATION *ET* UN UPLOAD : ToGPU fait CreateBuffer "
							"puis Upload. Les postes « reserve de tampons » et « supprimer les uploads » "
							"comptent donc les memes objets.");
				logger.Info("    ANCRE de resolution : NkGpuAncre a l'execution = {0}. Adresse de liaison "
							"donnee par : nm -C <exe> | grep NkGpuAncre. Site = liaison(ancre) + (retour - "
							"ancre).",
							(unsigned long long)(uintptr_t)&NkGpuAncre);
				logger.Info("    {0:>18} {1:>14} {2:>9} {3:>12} {4:>10}", "retour", "ecart/ancre", "appels",
							"Mo", "Ko/appel");
				for (uint32 k = 0; k < gBasculeN; ++k) {
					const NkGpuBasculeLigne &l = gBascule[ord[k]];
					const long long ecart = (long long)((intptr_t)l.retour - (intptr_t)&NkGpuAncre);
					logger.Info("    {0:>18} {1:>14} {2:>9} {3:>12.2f} {4:>10.1f}",
								(unsigned long long)(uintptr_t)l.retour, ecart, (long long)l.appels,
								l.octets / 1.0e6, l.octets / 1000.0 / (double)l.appels);
				}
				if (gBasculeHorsTable > 0)
					logger.Info("    ⚠️ {0} bascules HORS TABLE (plus de 64 sites distincts) — le compte "
								"total reste juste, l'attribution de celles-la est perdue.",
								(long long)gBasculeHorsTable);
			}
		}

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

		// Un noyau qui ecrit un tampon ENTIEREMENT NUL alors que son entree ne l'est
		// pas n'a rien calcule. C'est la signature exacte d'un shader qui n'a pas
		// compile : le tampon de sortie reste tel qu'il a ete alloue, et rien ne
		// remonte a l'appelant (constate le 13 aout 2026 sur RoPE, mot reserve GLSL).
		//
		// Controle fait UNE SEULE FOIS par noyau, a son premier usage : le cout est
		// d'une relecture par noyau et par processus (une trentaine en tout),
		// negligeable devant un entrainement, alors que le defaut qu'il attrape
		// couterait des heures de calcul sur des valeurs inchangees.
		//
		// On exige qu'une ENTREE soit non nulle avant de conclure : sans cela, un
		// noyau parfaitement correct nourri de zeros serait accuse a tort.
		static void NkGpuVerifierSortieNonNulle(NkTensorGpu &gpu, const char *nom, uint64 entree, uint64 sortie,
												uint32 count) {
			// ⚠ LA SORTIE SE LIT EN ENTIER, PAS PAR SON DEBUT.
			//
			// Premiere version : on echantillonnait les 4096 PREMIERS elements. Faux
			// des que la sortie est CREUSE. Cas vecu le 14 aout 2026 : `embedding_bwd`
			// produit le gradient de la table [V, d], ou seules les lignes des tokens
			// presents dans le lot sont non nulles. Les 4096 premieres valeurs couvrent
			// une dizaine de lignes du vocabulaire : si aucun de ces tokens n'est dans
			// le lot, elles sont LEGITIMEMENT nulles. Le garde-fou a donc signale un
			// defaut inexistant, et le filet de securite de l'entrainement a coupe une
			// course de 300 pas au 30e.
			//
			// Lire TOUTE la sortie supprime le probleme a la racine : un tampon jamais
			// ecrit est nul PARTOUT, tandis qu'une sortie creuse a toujours au moins une
			// valeur non nulle quelque part. Le cout reste d'une relecture par noyau et
			// par processus.
			const uint32 kMaxSortie = 67108864u; // 64 M valeurs = 256 Mo, borne de securite
			const uint32 nSortie = (count < kMaxSortie) ? count : kMaxSortie;
			if (nSortie == 0u)
				return;

			// L'entree, elle, garde un petit echantillon : elle sert seulement a etablir
			// « il y avait de la matiere a calculer ». On ne peut PAS y lire `count`
			// valeurs — les deux tampons n'ont pas la meme taille (pour embedding_bwd,
			// l'entree fait B*T*d et la sortie V*d) et la relecture deborderait.
			const uint32 kEchEntree = (count < 4096u) ? count : 4096u;
			NkVector<float> ein;
			ein.Resize((nk_size)kEchEntree);
			if (!gpu.Download(entree, ein.Data(), (nk_size)kEchEntree * sizeof(float)))
				return; // relecture impossible : on ne conclut pas, on ne crie pas non plus
			bool entreeNonNulle = false;
			for (uint32 i = 0; i < kEchEntree; ++i)
				if (ein[(nk_size)i] != 0.0f) {
					entreeNonNulle = true;
					break;
				}
			if (!entreeNonNulle)
				return; // rien a calculer : une sortie nulle serait normale

			NkVector<float> eout;
			eout.Resize((nk_size)nSortie);
			if (!gpu.Download(sortie, eout.Data(), (nk_size)nSortie * sizeof(float)))
				return;
			for (uint32 i = 0; i < nSortie; ++i)
				if (eout[(nk_size)i] != 0.0f)
					return; // au moins une valeur ecrite : le noyau a calcule

			NkGpuSignalerDefaut(nom, "sortie ENTIEREMENT NULLE sur une entree non nulle "
									 "(shader non compile ? indexation fausse ?)",
								(int64)count);
		}

		int64 NkTensorGpu::DefautCount() {
			return gGpuDefauts;
		}

		int64 NkTensorGpu::OpCount() {
			return gGpuOps;
		}

		int64 NkTensorGpu::VramPic() {
			return gVramPic; // PHYSIQUE : vivants + retenus par la reserve
		}

		int64 NkTensorGpu::VramPicCalcul() {
			return gVramPicCalcul; // tampons de calcul seuls, hors retention
		}

		int64 NkTensorGpu::VramVivante() {
			return gVramVivante;
		}

		void NkTensorGpu::RazVramPic() {
			gVramPic = gVramVivante + gReserveOctets;
			gVramPicCalcul = gVramVivante;
		}

		// ---- RESERVE DE TAMPONS : pilotage et TEMOIN ----------------------------
		// L'interrupteur existe pour que LEGACY et NEUF tournent depuis LE MEME
		// BINAIRE. C'est ce qui a rendu defendable la mesure du x1,57 sur le
		// chantier n°1 : rien d'autre ne change entre les deux bras, donc aucun
		// ecart de compilation ne peut se glisser dans le resultat.
		void NkTensorGpu::ReserveActive(bool actif) {
			if (!actif)
				ReserveVider(); // sinon on garderait de la VRAM immobilisee pour rien
			gReserveActive = actif;
		}
		bool NkTensorGpu::ReserveEstActive() {
			return gReserveActive;
		}
		void NkTensorGpu::ReserveBudget(int64 octetsMax) {
			gReserveBudget = octetsMax;
		}
		int64 NkTensorGpu::ReserveServis() {
			return gReserveServis;
		}
		int64 NkTensorGpu::ReserveNeufs() {
			return gReserveNeufs;
		}
		int64 NkTensorGpu::ReserveOctetsRetenus() {
			return gReserveOctets;
		}
		int64 NkTensorGpu::ReserveTamponsRetenus() {
			return gReserveTampons;
		}
		int64 NkTensorGpu::ReserveEvictions() {
			return gReserveEvictions;
		}
		void NkTensorGpu::ReserveRazCompteurs() {
			gReserveServis = 0;
			gReserveNeufs = 0;
			gReserveEvictions = 0;
		}

		void NkTensorGpu::ReserveVider() {
			NkTensorGpu &g = NkTensorGpu::Get();
			if (!g.mImpl || !g.mImpl->device)
				return;
			Impl *d = g.mImpl;
			d->reserve.ForEach([d](const uint64 &, NkVector<NkBufferHandle> &pile) {
				for (uint32 i = 0; i < pile.Size(); ++i)
					d->device->DestroyBuffer(pile[i]);
				pile.Clear();
			});
			d->reserve.Clear();
			gReserveOctets = 0;
			gReserveTampons = 0;
		}

		// ---- Buffers ------------------------------------------------------------
		uint64 NkTensorGpu::CreateBuffer(nk_size bytes) {
			NkGpuChrono chr("~alloc");
			chr.Travail(0.0, 0.0);
			if (!EnsureInit())
				return 0;

			// ⚠️ LA LIMITE QUI NE SE VOIT PAS. Un tampon peut être ALLOUÉ avec succès
			// et rester inaccessible au shader au-delà de `maxStorageBufferRange` :
			// l'allocation réussit, le dispatch part, et le calcul ne se fait pas —
			// sans la moindre erreur. C'est le profil exact de la panne constatée le
			// 2026-08-09 à B=24, où la perte restait collée à ln(vocabulaire) tandis
			// que le run paraissait 3,6× plus rapide. À B=24 le tenseur de logits
			// pèse 384 Mo, à B=12 il en pèse 201 : si la carte plafonne entre les
			// deux, tout s'explique.
			// On le dit maintenant, au lieu de le découvrir à la perte immobile.
			{
				const NkDeviceCaps &caps = mImpl->device->GetCaps();
				if (caps.maxStorageBufferRange > 0 && (uint64)bytes > (uint64)caps.maxStorageBufferRange) {
					NkGpuSignalerDefaut("CreateBuffer",
										"tampon PLUS GRAND que ce qu'un shader peut adresser sur cette carte "
										"(maxStorageBufferRange) — reduire le lot",
										(int64)bytes);
					return 0;
				}
			}

			// ---- RESERVE : servir un tampon deja alloue, de TAILLE EXACTE ---------
			// ⚠️ Le compteur est incremente ICI et NULLE PART AILLEURS. C'est lui qui
			// distingue « servi depuis la reserve » de « alloue a neuf » — sans quoi
			// un gain mesure ne prouverait rien, une reserve repondant toujours.
			NkBufferHandle h{};
			bool servi = false;
			if (gReserveActive) {
				auto *pile = mImpl->reserve.Find((uint64)bytes);
				if (pile && pile->Size() > 0) {
					h = (*pile)[pile->Size() - 1];
					pile->PopBack();
					servi = true;
					++gReserveServis;
					gReserveOctets -= (int64)bytes;
					--gReserveTampons;
				}
			}

			if (!servi) {
				h = mImpl->device->CreateBuffer(NkBufferDesc::Storage(bytes, false));
				++gReserveNeufs;
			}
			if (!h.IsValid()) {
				NkGpuSignalerDefaut("CreateBuffer", "allocation refusee, octets demandes", (int64)bytes);
				return 0;
			}
			uint64 id = mImpl->nextId++;
			mImpl->buffers.Insert(id, h);
			mImpl->tailles.Insert(id, bytes);
			gVramVivante += (int64)bytes;
			if (gVramVivante > gVramPicCalcul)
				gVramPicCalcul = gVramVivante;
			// Le pic PHYSIQUE compte aussi ce que la reserve retient. C'est ICI le
			// seul endroit ou l'occupation physique peut monter : la retention est
			// un TRANSFERT (vivant -> retenu, total inchange), servir depuis la
			// reserve aussi (retenu -> vivant), l'eviction et la destruction font
			// baisser. Nul besoin de toucher au pic dans DestroyBuffer.
			const int64 physique = gVramVivante + gReserveOctets;
			if (physique > gVramPic)
				gVramPic = physique;
			return id;
		}

		void NkTensorGpu::DestroyBuffer(uint64 id) {
			NkGpuChrono chr("~free");
			chr.Travail(0.0, 0.0);
			if (!mImpl || !mImpl->device || id == 0)
				return;
			auto *h = mImpl->buffers.Find(id);
			if (h) {
				// ---- RESERVE : retenir au lieu de detruire, dans la limite du budget --
				// ⚠️ Le budget existe parce qu'une reserve NE LIBERE PLUS LA VRAM. Le
				// pic mesure est de 6 659 Mo sur 8 Go : sans plafond, retenir suffirait
				// a faire deborder, et l'echec se presenterait comme une allocation
				// refusee loin d'ici. Au-dela du budget on detruit VRAIMENT, et on
				// compte l'eviction pour que le rapport le dise.
				bool retenu = false;
				if (gReserveActive) {
					auto *t = mImpl->tailles.Find(id);
					const int64 oct = t ? (int64)*t : 0;
					if (oct > 0 && gReserveOctets + oct <= gReserveBudget) {
						auto *pile = mImpl->reserve.Find((uint64)oct);
						if (!pile) {
							mImpl->reserve.Insert((uint64)oct, NkVector<NkBufferHandle>{});
							pile = mImpl->reserve.Find((uint64)oct);
						}
						if (pile) {
							pile->PushBack(*h);
							gReserveOctets += oct;
							++gReserveTampons;
							retenu = true;
						}
					} else if (oct > 0) {
						++gReserveEvictions;
					}
				}
				if (!retenu)
					mImpl->device->DestroyBuffer(*h);
				mImpl->buffers.Erase(id);
				// Decompte APRES la destruction reussie : compter une liberation qui
				// n'a pas eu lieu ferait mentir le pic dans le sens rassurant.
				auto *t = mImpl->tailles.Find(id);
				if (t) {
					gVramVivante -= (int64)*t;
					mImpl->tailles.Erase(id);
				}
			}
		}

		bool NkTensorGpu::Upload(uint64 id, const void *data, nk_size bytes) {
			NkGpuChrono chr("~upload");
			chr.Travail(0.0, (double)bytes);
			if (!mImpl || !mImpl->device)
				return false;
			NkBufferHandle h = mImpl->Handle(id);
			if (!h.IsValid())
				return false;
			return mImpl->device->WriteBuffer(h, data, bytes);
		}

		bool NkTensorGpu::Download(uint64 id, void *out, nk_size bytes) {
			NkGpuChrono chr("~download");
			chr.Travail(0.0, (double)bytes);
			if (!mImpl || !mImpl->device)
				return false;
			NkBufferHandle h = mImpl->Handle(id);
			if (!h.IsValid())
				return false;
			return mImpl->device->ReadBuffer(h, out, bytes);
		}

		// ---- Remise a zero SUR PLACE (pas de transfert depuis l'hote) ------------
		// Le poste `~clear` est instrumente comme `~upload` : c'est exactement le
		// poste qui doit le remplacer dans le profil, et on veut pouvoir les
		// comparer ligne a ligne au lieu de le deduire.
		bool NkTensorGpu::Clear(uint64 id, nk_size bytes, uint32 motif) {
			NkGpuChrono chr("~clear");
			chr.Travail(0.0, (double)bytes); // trafic ECRIT cote GPU (rien ne traverse le bus)
			if (!mImpl || !mImpl->device)
				return false;
			NkBufferHandle h = mImpl->Handle(id);
			if (!h.IsValid())
				return false;

			// `size` doit etre un multiple de 4 pour vkCmdFillBuffer ; nos tampons de
			// tenseurs sont des multiples de 4 octets (f32/i32). On refuse le reste
			// plutot que de nettoyer une plage tronquee.
			if ((bytes % 4) != 0)
				return false;

			auto *cmd = mImpl->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
			if (!cmd)
				return false;
			cmd->Begin();
			cmd->ClearBuffer(h, motif, 0, (uint64)bytes);
			cmd->End();
			mImpl->device->Submit(&cmd, 1);
			// Meme discipline que les dispatches : on attend avant de rendre le
			// tampon a l'appelant. Supprimer ce WaitIdle est le chantier n°3 (un
			// tampon de commandes par pas), pas celui-ci.
			mImpl->device->WaitIdle();
			mImpl->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// ⚠️ TEMOIN, PAS DECLARATION. `NkICommandBuffer::ClearBuffer` a existe des
		// mois avec un corps vide et zero surcharge : la signature etait la, deux
		// appelants s'en servaient, et elle ne mettait rien a zero nulle part. Un
		// `grep` du nom plus un appelant avaient suffi a la croire implementee — un
		// appelant ne prouve rien sur l'appele.
		// Donc on ECRIT un motif non nul, on appelle Clear, on RELIT, et on exige
		// des zeros. Le resultat est calcule une seule fois et journalise.
		bool NkTensorGpu::ClearDisponible() {
			static int etat = -1; // -1 = pas encore mesure, 0 = non, 1 = oui
			if (etat >= 0)
				return etat == 1;
			etat = 0;

			NkTensorGpu &g = Get();
			if (!g.IsAvailable())
				return false;

			const nk_size n = 64;
			const nk_size octets = n * sizeof(uint32);
			uint64 buf = g.CreateBuffer(octets);
			if (!buf) {
				logger_src.Warnf("[NkTensorGpu] temoin ClearBuffer : allocation refusee, remise a zero GPU "
								 "declaree INDISPONIBLE (repli sur le chemin historique).");
				return false;
			}

			uint32 motif[n];
			for (nk_size i = 0; i < n; ++i)
				motif[i] = 0xDEADBEEFu;
			bool ok = g.Upload(buf, motif, octets);
			ok = ok && g.Clear(buf, octets, 0);

			uint32 relu[n];
			for (nk_size i = 0; i < n; ++i)
				relu[i] = 0xFFFFFFFFu; // valeur de depart differente de 0 ET du motif
			ok = ok && g.Download(buf, relu, octets);
			if (ok)
				for (nk_size i = 0; i < n; ++i)
					if (relu[i] != 0u) {
						ok = false;
						break;
					}
			g.DestroyBuffer(buf);

			etat = ok ? 1 : 0;
			if (ok)
				logger_src.Infof("[NkTensorGpu] temoin ClearBuffer : OK sur %s — la remise a zero se fait SUR "
								 "le GPU, plus aucun tenseur de zeros ne transite par le bus.",
								 g.BackendName());
			else
				logger_src.Warnf("[NkTensorGpu] temoin ClearBuffer : ECHEC sur %s — la primitive ne met pas le "
								 "tampon a zero. Repli sur le chemin historique (fabrication CPU + upload), "
								 "correct mais couteux. Ce backend n'a pas de surcharge de ClearBuffer.",
								 g.BackendName());
			return etat == 1;
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
			++gGpuOps;
			NkGpuChrono chr(name);
			chr.Travail((double)count, 12.0 * (double)count); // 2 lectures + 1 ecriture f32
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
			if (!k->verifie) {
				k->verifie = true;
				NkGpuVerifierSortieNonNulle(*this, name, a, c, count);
			}
			d->device->WaitIdle(); // flush avant le Download (ReadBuffer synchronise aussi via Map)

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		bool NkTensorGpu::RunUnary(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint32 count) {
			++gGpuOps;
			NkGpuChrono chr(name);
			chr.Travail((double)count, 8.0 * (double)count); // 1 lecture + 1 ecriture f32
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
			if (!k->verifie) {
				k->verifie = true;
				NkGpuVerifierSortieNonNulle(*this, name, a, b, count);
			}
			d->device->WaitIdle(); // flush avant le Download (ReadBuffer synchronise aussi via Map)

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		bool NkTensorGpu::RunUnaryScalar(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint32 count,
										 float s) {
			++gGpuOps;
			NkGpuChrono chr(name);
			chr.Travail((double)count, 8.0 * (double)count); // 1 lecture + 1 ecriture f32
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
			++gGpuOps;
			NkGpuChrono chr(name);
			chr.Travail((double)outer * (double)reduce * (double)inner,
						4.0 * ((double)outer * (double)reduce * (double)inner + (double)outer * (double)inner));
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
			++gGpuOps;
			NkGpuChrono chr("gather");
			chr.Travail(0.0, 8.0 * (double)count); // pur deplacement : aucun FLOP utile
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
			++gGpuOps;
			NkGpuChrono chr(name);
			chr.Travail(0.0, 8.0 * (double)count);
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
			if (!k->verifie) {
				k->verifie = true;
				NkGpuVerifierSortieNonNulle(*this, name, in, out, count);
			}
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// Générique 3 buffers (a,b,c) + UBO {12 uints} binding 3.
		bool NkTensorGpu::RunOp3(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint64 c,
								 const uint32 *p12, uint32 count) {
			++gGpuOps;
			NkGpuChrono chr(name);
			chr.Travail((double)count, 12.0 * (double)count);
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
			if (!k->verifie) {
				k->verifie = true;
				NkGpuVerifierSortieNonNulle(*this, name, a, c, count);
			}
			d->device->WaitIdle();

			d->device->FreeDescriptorSet(set);
			d->device->DestroyCommandBuffer(cmd);
			return true;
		}

		// Pas d'Adam fusé : buffers 0,1,2,3 (param,grad,m,v) + UBO binding 4. Tout en place.
		bool NkTensorGpu::RunAdam(uint64 param, uint64 grad, uint64 m, uint64 v, uint32 count, float lr, float b1,
								  float b2, float eps, float b1t, float b2t, float wd) {
			// ⚠️ CE COMPTEUR MANQUAIT (trouve le 2026-08-16). RunAdam est le SEUL
			// dispatch qui ne s'annoncait pas : `OpCount()` ignorait un lancement par
			// tenseur de parametres et par pas, et le « cout fixe par operation »
			// journalise au pas 30 divisait donc le temps par un nombre trop petit —
			// il SURESTIMAIT ce cout. Portee bornee : +1 operation par tenseur de
			// parametres et par pas.
			++gGpuOps;
			NkGpuChrono chr("adam");
			chr.Travail(11.0 * (double)count, 28.0 * (double)count); // param/m/v lus+ecrits, grad lu
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

		// ---- MatMul PAVÉ EN REGISTRES ------------------------------------------
		//
		// LE PROBLÈME DU NOYAU NAÏF (conservé plus bas comme repli). Un fil par
		// élément de sortie relit toute une ligne de A et toute une colonne de B
		// depuis la mémoire globale. Le trafic vaut donc 2·M·N·K flottants. Pour la
		// tête de sortie d'Ilyana — [3072,384] × [384,16385] — cela fait
		// **154 Go de lectures** pour UN produit. À ~450 Go/s, 0,34 s rien que pour
		// bouger les octets, et plusieurs de ces produits par pas. C'est ce qui
		// explique une carte occupée mais à 30 W : elle attend la mémoire, elle ne
		// calcule pas.
		//
		// LE REMÈDE ICI. Chaque fil calcule un bloc de 4 lignes × 4 colonnes. La
		// valeur A[row+i, k] chargée une fois sert aux 4 colonnes, et B[k, col+j]
		// aux 4 lignes : le trafic tombe d'un facteur 4 (2·M·N·K/4). On reste en
		// dispatch 1D et sans mémoire partagée — donc sans barrière, sans course
		// possible, et le repli naïf reste disponible pour les formes qui ne s'y
		// prêtent pas. La version pavée en mémoire partagée (encore 4× de moins)
		// viendra ensuite : NkSL sait faire `shared` et `barrier()`.
		static const char *kMatMulT4NkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Dims { uint M; uint N; uint K; } d;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint blocsN = (d.N + 3u) / 4u;
    uint idx = gl_GlobalInvocationID.x;
    uint blocsM = (d.M + 3u) / 4u;
    if (idx < blocsM * blocsN) {
        uint br = idx / blocsN;
        uint bc = idx - br * blocsN;
        uint row0 = br * 4u;
        uint col0 = bc * 4u;
        float acc00 = 0.0; float acc01 = 0.0; float acc02 = 0.0; float acc03 = 0.0;
        float acc10 = 0.0; float acc11 = 0.0; float acc12 = 0.0; float acc13 = 0.0;
        float acc20 = 0.0; float acc21 = 0.0; float acc22 = 0.0; float acc23 = 0.0;
        float acc30 = 0.0; float acc31 = 0.0; float acc32 = 0.0; float acc33 = 0.0;
        for (uint k = 0u; k < d.K; k = k + 1u) {
            float b0 = 0.0; float b1 = 0.0; float b2 = 0.0; float b3 = 0.0;
            if (col0 + 0u < d.N) { b0 = B.data[k * d.N + col0 + 0u]; }
            if (col0 + 1u < d.N) { b1 = B.data[k * d.N + col0 + 1u]; }
            if (col0 + 2u < d.N) { b2 = B.data[k * d.N + col0 + 2u]; }
            if (col0 + 3u < d.N) { b3 = B.data[k * d.N + col0 + 3u]; }
            if (row0 + 0u < d.M) {
                float a = A.data[(row0 + 0u) * d.K + k];
                acc00 = acc00 + a * b0; acc01 = acc01 + a * b1;
                acc02 = acc02 + a * b2; acc03 = acc03 + a * b3;
            }
            if (row0 + 1u < d.M) {
                float a = A.data[(row0 + 1u) * d.K + k];
                acc10 = acc10 + a * b0; acc11 = acc11 + a * b1;
                acc12 = acc12 + a * b2; acc13 = acc13 + a * b3;
            }
            if (row0 + 2u < d.M) {
                float a = A.data[(row0 + 2u) * d.K + k];
                acc20 = acc20 + a * b0; acc21 = acc21 + a * b1;
                acc22 = acc22 + a * b2; acc23 = acc23 + a * b3;
            }
            if (row0 + 3u < d.M) {
                float a = A.data[(row0 + 3u) * d.K + k];
                acc30 = acc30 + a * b0; acc31 = acc31 + a * b1;
                acc32 = acc32 + a * b2; acc33 = acc33 + a * b3;
            }
        }
        if (row0 + 0u < d.M) {
            if (col0 + 0u < d.N) { C.data[(row0 + 0u) * d.N + col0 + 0u] = acc00; }
            if (col0 + 1u < d.N) { C.data[(row0 + 0u) * d.N + col0 + 1u] = acc01; }
            if (col0 + 2u < d.N) { C.data[(row0 + 0u) * d.N + col0 + 2u] = acc02; }
            if (col0 + 3u < d.N) { C.data[(row0 + 0u) * d.N + col0 + 3u] = acc03; }
        }
        if (row0 + 1u < d.M) {
            if (col0 + 0u < d.N) { C.data[(row0 + 1u) * d.N + col0 + 0u] = acc10; }
            if (col0 + 1u < d.N) { C.data[(row0 + 1u) * d.N + col0 + 1u] = acc11; }
            if (col0 + 2u < d.N) { C.data[(row0 + 1u) * d.N + col0 + 2u] = acc12; }
            if (col0 + 3u < d.N) { C.data[(row0 + 1u) * d.N + col0 + 3u] = acc13; }
        }
        if (row0 + 2u < d.M) {
            if (col0 + 0u < d.N) { C.data[(row0 + 2u) * d.N + col0 + 0u] = acc20; }
            if (col0 + 1u < d.N) { C.data[(row0 + 2u) * d.N + col0 + 1u] = acc21; }
            if (col0 + 2u < d.N) { C.data[(row0 + 2u) * d.N + col0 + 2u] = acc22; }
            if (col0 + 3u < d.N) { C.data[(row0 + 2u) * d.N + col0 + 3u] = acc23; }
        }
        if (row0 + 3u < d.M) {
            if (col0 + 0u < d.N) { C.data[(row0 + 3u) * d.N + col0 + 0u] = acc30; }
            if (col0 + 1u < d.N) { C.data[(row0 + 3u) * d.N + col0 + 1u] = acc31; }
            if (col0 + 2u < d.N) { C.data[(row0 + 3u) * d.N + col0 + 2u] = acc32; }
            if (col0 + 3u < d.N) { C.data[(row0 + 3u) * d.N + col0 + 3u] = acc33; }
        }
    }
}
)NKSL";

		// MatMul : buffers 0,1,2 (A,B,C) + UBO { uint M,N,K } binding 3.
		// Dispatch 1D (index plat) : chaque thread calcule un élément C[idx]. On
		// évite le workgroup 2D (course intermittente observée sur WARP headless).
		// CONSERVÉ comme repli et comme référence de correction.
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
			++gGpuOps;
			// Le NOM porte la variante : `matmul` (naif) et `matmul_t4` (pave 4x4) ont
			// des intensites arithmetiques differentes d'un facteur 4, donc des plafonds
			// differents. Les confondre dans une seule ligne effacerait justement ce
			// qu'on cherche a mesurer.
			NkGpuChrono chr((((uint64)M * (uint64)N >= 65536ull) && K >= 16u) ? "matmul_t4" : "matmul");
			chr.Travail(2.0 * (double)M * (double)N * (double)K,
						4.0 * ((double)M * (double)K + (double)K * (double)N + (double)M * (double)N));
			if (!EnsureInit())
				return false;
			Impl *d = mImpl;
			// Le noyau pavé ne paie que sur des produits assez gros pour que le
			// trafic mémoire domine ; sur de petites matrices, ses tests de bornes
			// coûtent plus qu'ils ne rapportent. Seuil volontairement prudent.
			const bool pave = ((uint64)M * (uint64)N >= 65536ull) && K >= 16u;
			Impl::Kernel *k = pave
								  ? d->GetOrCompile("matmul_t4", NkString(kMatMulT4NkSL), /*nBuffers*/ 3, /*ubo*/ 3)
								  : d->GetOrCompile("matmul", NkString(kMatMulNkSL), /*nBuffers*/ 3, /*ubo*/ 3);
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
			// Pavé : un fil pour un bloc 4×4, donc seize fois moins de fils.
			const uint64 fils = pave ? ((uint64)((M + 3u) / 4u) * (uint64)((N + 3u) / 4u)) : ((uint64)M * (uint64)N);
			cmd->Dispatch((uint32)((fils + 63ull) / 64ull), 1, 1);
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

		// =====================================================================
		// Zeros fabriques DIRECTEMENT sur le GPU — le remede du chantier n°1.
		//
		// Avant : `ToDevOf(NkTensor::Zeros(shape), ref)` fabriquait les zeros sur
		// CPU puis les montait. Mesure du 16/08 : 12,77 Go par pas, 99,9 % de tout
		// le trafic CPU->GPU de l'entrainement, depuis la SEULE ligne
		// `NkVar.cpp:1249`. Il n'y a aucune information dans ce transfert.
		//
		// ⚠️ Ne renvoie JAMAIS un tampon au contenu indetermine. Si la remise a zero
		// echoue, on detruit le tampon et on renvoie un tenseur invalide : l'appelant
		// doit pouvoir se replier. Des gradients faux ne se distinguent pas de
		// gradients justes — c'est precisement pour ca qu'on ne devine pas.
		// =====================================================================
		NkTensor NkGpuZeros(const NkShape &shape, NkDType dtype) {
			if (!NkTensorGpu::Get().IsAvailable() || !NkTensorGpu::ClearDisponible())
				return NkTensor{};
			const int64 n = NkShapeNumel(shape);
			const nk_size bytes = (nk_size)(n < 0 ? 0 : n) * NkDTypeSize(dtype);
			if (bytes == 0)
				return NkTensor{};
			uint64 buf = NkTensorGpu::Get().CreateBuffer(bytes);
			if (!buf)
				return NkTensor{};
			if (!NkTensorGpu::Get().Clear(buf, bytes, 0)) {
				NkTensorGpu::Get().DestroyBuffer(buf);
				NkGpuSignalerDefaut("NkGpuZeros", "remise a zero GPU refusee apres allocation, octets", (int64)bytes);
				return NkTensor{};
			}
			return NkTensorInternal::MakeGpu(shape, dtype, buf);
		}

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

		// ---- Entropie croisée à cible par INDICES, sur GPU -----------------------
		//
		// POURQUOI CES DEUX NOYAUX. La perte et son gradient étaient calculés sur
		// CPU : cela RAPATRIAIT tout le tenseur de logits à chaque micro-lot. À
		// B=12, T=256, vocabulaire 16385, ce sont 201 Mo qui descendent, un softmax
		// mono-thread sur 50 millions d'éléments, puis 201 Mo qui remontent — deux
		// fois par pas. Le GPU passait 70 % de son temps à attendre le CPU (mesuré :
		// 28-41 % d'occupation, 30-70 W sur une carte qui en encaisse 220).
		// Ici tout reste sur la carte : seuls B flottants redescendent (la perte par
		// ligne) au lieu de B×vocabulaire.

		// Gradient : dLogits[b,c] = (probs[b,c] − [c == cible[b]]) × coef.
		// Cible négative = ligne MASQUÉE : gradient nul.
		static const char *kCeIdxBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufT { float data[]; } T;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform Params { uint count; uint C; float coef; uint pad; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        uint b = i / pc.C;
        uint c = i - b * pc.C;
        float t = T.data[b];
        if (t < 0.0) { O.data[i] = 0.0; }
        else {
            uint idx = uint(t + 0.5);
            float v = P.data[i];
            if (c == idx) { v = v - 1.0; }
            O.data[i] = v * pc.coef;
        }
    }
}
)NKSL";

		// Perte par ligne : L[b] = −log(probs[b, cible[b]]), 0 si masquée.
		static const char *kCeIdxFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufT { float data[]; } T;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform Params { uint count; uint C; uint pad0; uint pad1; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint b = gl_GlobalInvocationID.x;
    if (b < pc.count) {
        float t = T.data[b];
        if (t < 0.0) { O.data[b] = 0.0; }
        else {
            uint idx = uint(t + 0.5);
            float p = P.data[b * pc.C + idx];
            O.data[b] = -log(p + 1e-12);
        }
    }
}
)NKSL";

		NkTensor NkGpuCeIdxForward(const NkTensor &probs, const NkTensor &cibles) {
			NkTensor gp = (probs.Device() == NkDevice::NK_GPU) ? probs : probs.ToGPU();
			NkTensor gt = (cibles.Device() == NkDevice::NK_GPU) ? cibles : cibles.ToGPU();
			if (!gp.IsValid() || !gt.IsValid())
				return NkTensor{};
			const NkShape &sh = gp.Shape();
			const int64 B = sh.Size() >= 1 ? sh[0] : 1;
			const int64 C = sh.Size() >= 2 ? sh[1] : gp.Numel();
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)B * NkDTypeSize(gp.DType()));
			if (!ob)
				return NkTensor{};

			// ⚠️ EMPOISONNER LE TAMPON, NE PAS LE METTRE A ZERO.
			//
			// Une ligne que le noyau n'ecrit pas garde ce qui trainait la. Si c'est
			// ZERO, elle ressemble a une perte tres basse -- plausible, donc invisible.
			// Si c'est le RESTE DU PAS PRECEDENT, c'est pire encore : une vraie valeur
			// de perte, indetectable par construction.
			//
			// Une entropie croisee vaut -log(p) avec 0 < p <= 1 : elle est FINIE et
			// POSITIVE. Un NaN est donc hors domaine, quelle que soit la configuration
			// et quel que soit le mecanisme de la panne. On remplace ainsi la regle
			// « zero = defaut », vraie d'un seul cas, par « hors domaine = defaut »,
			// qui est strictement plus forte et ne se perime pas.
			//
			// Motivation mesuree (14 aout 2026) : un run a rendu le QUART de la perte
			// de son jumeau parce que trois lignes sur quatre n'etaient pas calculees,
			// sans qu'aucune erreur ne soit signalee. A 8 % de lignes manquantes, une
			// telle perte ne ressemble pas a un defaut : elle ressemble a un PROGRES.
			{
				NkVector<float> poison;
				poison.Resize((nk_size)B);
				const uint32 kNaN = 0x7FC00000u; // NaN silencieux
				for (int64 i = 0; i < B; ++i) {
					float v;
					const unsigned char *s = (const unsigned char *)&kNaN;
					unsigned char *d = (unsigned char *)&v;
					for (int k = 0; k < 4; ++k)
						d[k] = s[k];
					poison[(nk_size)i] = v;
				}
				NkTensorGpu::Get().Upload(ob, poison.Data(), (nk_size)B * sizeof(float));
			}

			uint32 p[12] = {(uint32)B, (uint32)C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("ce_idx_fwd", NkString(kCeIdxFwdNkSL), NkTensorInternal::GpuBuffer(gp),
									  NkTensorInternal::GpuBuffer(gt), ob, p, (uint32)B);
			return NkTensorInternal::MakeGpu(NkShape{B}, gp.DType(), ob);
		}

		NkTensor NkGpuCeIdxBackward(const NkTensor &probs, const NkTensor &cibles, double coef) {
			NkTensor gp = (probs.Device() == NkDevice::NK_GPU) ? probs : probs.ToGPU();
			NkTensor gt = (cibles.Device() == NkDevice::NK_GPU) ? cibles : cibles.ToGPU();
			if (!gp.IsValid() || !gt.IsValid())
				return NkTensor{};
			const NkShape &sh = gp.Shape();
			const int64 B = sh.Size() >= 1 ? sh[0] : 1;
			const int64 C = sh.Size() >= 2 ? sh[1] : gp.Numel();
			const int64 n = B * C;
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(gp.DType()));
			if (!ob)
				return NkTensor{};
			// `coef` traverse le bloc de paramètres (déclaré en uint côté hôte) : on
			// recopie ses octets tels quels, le shader le relit en float.
			const float coefF = (float)coef;
			uint32 coefBits = 0;
			const unsigned char *src = (const unsigned char *)&coefF;
			unsigned char *dst = (unsigned char *)&coefBits;
			for (int k = 0; k < 4; ++k)
				dst[k] = src[k];
			uint32 p[12] = {(uint32)n, (uint32)C, coefBits, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("ce_idx_bwd", NkString(kCeIdxBwdNkSL), NkTensorInternal::GpuBuffer(gp),
									  NkTensorInternal::GpuBuffer(gt), ob, p, (uint32)n);
			return NkTensorInternal::MakeGpu(gp.Shape(), gp.DType(), ob);
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

		// ---- RMSNorm (dernier axe) GPU : fwd (x->y) + bwd (x,g->dx) ---------------
		// y_j = x_j / sqrt(mean(x²) + eps).  Pas de moyenne a retrancher : c'est ce
		// qui la rend moins chere que LayerNorm.
		static const char *kRmsNormFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) uniform P { uint rows; uint D; float eps; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.D; float fD = float(d.D);
        float ms = 0.0;
        for (uint c=0u;c<d.D;c=c+1u) { float v = A.data[base+c]; ms = ms + v*v; }
        ms = ms / fD;
        float inv = 1.0 / sqrt(ms + d.eps);
        for (uint c=0u;c<d.D;c=c+1u) B.data[base+c] = A.data[base+c] * inv;
    }
}
)NKSL";
		// dx_j = inv·dy_j − (inv³/d)·x_j·Σ_i(x_i·dy_i)
		static const char *kRmsNormBwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufG { float data[]; } G;
@binding(set=0, binding=2) buffer BufD { float data[]; } DX;
@binding(set=0, binding=3) uniform P { uint rows; uint D; float eps; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.D; float fD = float(d.D);
        float ms = 0.0; float dot = 0.0;
        for (uint c=0u;c<d.D;c=c+1u) {
            float v = X.data[base+c];
            ms = ms + v*v;
            dot = dot + v * G.data[base+c];
        }
        ms = ms / fD;
        float inv = 1.0 / sqrt(ms + d.eps);
        float coef = inv * inv * inv * dot / fD;
        for (uint c=0u;c<d.D;c=c+1u)
            DX.data[base+c] = inv * G.data[base+c] - coef * X.data[base+c];
    }
}
)NKSL";

		// ---- RoPE GPU : rotation par paires (i, i+moitie) du dernier axe ---------
		//
		// Les cosinus/sinus sont LUS DANS UNE TABLE fournie par l'appelant, ils ne
		// sont PAS recalcules ici. Deux raisons, et la seconde est la vraie :
		//  1. la table supprime pow/cos/sin de la boucle interne ;
		//  2. en flottant simple, l'angle pos*freq atteint ~256 radians, dont l'ulp
		//     vaut ~1.5e-5 : un noyau qui calculerait l'angle lui-meme s'ecarterait
		//     du chemin CPU (qui calcule en double) BIEN AU-DELA de la tolerance,
		//     et le test d'equivalence echouerait pour une raison etrangere a ce
		//     qu'il verifie. La table est construite en double, une seule fois.
		//
		// Disposition de la table : pour t dans [0,T), i dans [0,moitie) ->
		// TAB[2*(t*moitie+i)] = cos, TAB[2*(t*moitie+i)+1] = sin.
		// `sens` vaut +1 en avant, −1 en arriere (rotation transposee).
		//
		// ⚠ NOMS DES CHAMPS : `half` est un MOT RESERVE en GLSL et un TYPE en HLSL.
		// Le shader ne compilait pas, le noyau ne tournait pas, et le tampon de sortie
		// restait a ZERO -- sans la moindre erreur remontee jusqu'a l'appelant. C'est le
		// test d'equivalence qui l'a vu (ecart relatif 1,0 et norme non conservee).
		// Meme famille que `line`, deja rencontre en HLSL. D'ou `moitie` et `tlen`.
		static const char *kRoPENkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufT { float data[]; } TAB;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform P { uint rows; uint hd; uint tlen; uint moitie; float sens; uint pad; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < d.rows) {
        uint base = r * d.hd;
        uint t = r - (r / d.tlen) * d.tlen;
        for (uint i=0u;i<d.moitie;i=i+1u) {
            uint k = 2u * (t * d.moitie + i);
            float c = TAB.data[k];
            float sn = TAB.data[k+1u] * d.sens;
            float x0 = X.data[base+i];
            float x1 = X.data[base+i+d.moitie];
            O.data[base+i] = x0 * c - x1 * sn;
            O.data[base+i+d.moitie] = x0 * sn + x1 * c;
        }
        // Dimension de tete impaire : la composante orpheline passe telle quelle
        // plutot que d'etre perdue en silence (meme regle que le chemin CPU).
        if (d.hd - 2u * d.moitie == 1u) O.data[base+d.hd-1u] = X.data[base+d.hd-1u];
    }
}
)NKSL";

		// ---- SwiGLU GPU : h = silu(g) ⊙ u, silu(g) = g·σ(g) ----------------------
		static const char *kSwiGLUFwdNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufU { float data[]; } U;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform P { uint count; uint pad0; uint pad1; uint pad2; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < d.count) {
        float gv = G.data[i];
        float sig = 1.0 / (1.0 + exp(-gv));
        O.data[i] = gv * sig * U.data[i];
    }
}
)NKSL";
		// dU = dh ⊙ silu(g). Ne depend que de (g, dh) : deux entrees suffisent.
		static const char *kSwiGLUBwdDuNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufH { float data[]; } H;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform P { uint count; uint pad0; uint pad1; uint pad2; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < d.count) {
        float gv = G.data[i];
        float sig = 1.0 / (1.0 + exp(-gv));
        O.data[i] = H.data[i] * (gv * sig);
    }
}
)NKSL";
		// dG = (dh ⊙ u) ⊙ silu'(g), avec silu'(g) = σ(g)·(1 + g·(1−σ(g))).
		// L'appelant fournit deja le produit dh⊙u : le noyau reste a deux entrees,
		// ce qui evite d'ajouter un lanceur a quatre tampons pour un seul usage.
		static const char *kSwiGLUBwdDgNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufT { float data[]; } T;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform P { uint count; uint pad0; uint pad1; uint pad2; } d;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < d.count) {
        float gv = G.data[i];
        float sig = 1.0 / (1.0 + exp(-gv));
        float dsilu = sig * (1.0 + gv * (1.0 - sig));
        O.data[i] = T.data[i] * dsilu;
    }
}
)NKSL";

		// Recopie les octets d'un float dans un emplacement uint32 du bloc de
		// parametres (declare en uint cote hote, relu en float par le shader).
		static uint32 BitsOf(float f) {
			uint32 bits = 0;
			const unsigned char *src = (const unsigned char *)&f;
			unsigned char *dst = (unsigned char *)&bits;
			for (int k = 0; k < 4; ++k)
				dst[k] = src[k];
			return bits;
		}

		NkTensor NkGpuRmsNorm(const NkTensor &x, double eps) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			if (!gx.IsValid() || gx.Rank() < 1)
				return NkTensor{};
			const int64 D = gx.Shape()[gx.Rank() - 1];
			const int64 rows = (D > 0) ? gx.Numel() / D : 0;
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)gx.Numel() * NkDTypeSize(gx.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)D, BitsOf((float)eps), 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunConvOp("rmsnorm_fwd", NkString(kRmsNormFwdNkSL), NkTensorInternal::GpuBuffer(gx), ob,
										 p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), ob);
		}

		NkTensor NkGpuRmsNormBackward(const NkTensor &x, const NkTensor &grad, double eps) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gg = (grad.Device() == NkDevice::NK_GPU) ? grad : grad.ToGPU();
			if (!gx.IsValid() || !gg.IsValid() || gx.Rank() < 1)
				return NkTensor{};
			const int64 D = gx.Shape()[gx.Rank() - 1];
			const int64 rows = (D > 0) ? gx.Numel() / D : 0;
			uint64 db = NkTensorGpu::Get().CreateBuffer((nk_size)gx.Numel() * NkDTypeSize(gx.DType()));
			if (!db)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows, (uint32)D, BitsOf((float)eps), 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("rmsnorm_bwd", NkString(kRmsNormBwdNkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gg), db, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), db);
		}

		// `table` : tenseur [T * moitie * 2] construit par l'appelant (en double).
		NkTensor NkGpuRoPE(const NkTensor &x, const NkTensor &table, double sens) {
			NkTensor gx = (x.Device() == NkDevice::NK_GPU) ? x : x.ToGPU();
			NkTensor gt = (table.Device() == NkDevice::NK_GPU) ? table : table.ToGPU();
			if (!gx.IsValid() || !gt.IsValid() || gx.Rank() < 1)
				return NkTensor{};
			const int64 hd = gx.Shape()[gx.Rank() - 1];
			const int64 T = (gx.Rank() >= 2) ? gx.Shape()[gx.Rank() - 2] : 1;
			const int64 half = hd / 2;
			const int64 rows = (hd > 0) ? gx.Numel() / hd : 0;
			if (rows <= 0 || T <= 0)
				return NkTensor{};
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)gx.Numel() * NkDTypeSize(gx.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)rows,		(uint32)hd, (uint32)T, (uint32)half, BitsOf((float)sens), 0, 0, 0, 0,
							0,					0,			0};
			NkTensorGpu::Get().RunOp3("rope", NkString(kRoPENkSL), NkTensorInternal::GpuBuffer(gx),
									  NkTensorInternal::GpuBuffer(gt), ob, p, (uint32)rows);
			return NkTensorInternal::MakeGpu(gx.Shape(), gx.DType(), ob);
		}

		NkTensor NkGpuSwiGLU(const NkTensor &gate, const NkTensor &up) {
			NkTensor gg = (gate.Device() == NkDevice::NK_GPU) ? gate : gate.ToGPU();
			NkTensor gu = (up.Device() == NkDevice::NK_GPU) ? up : up.ToGPU();
			if (!gg.IsValid() || !gu.IsValid())
				return NkTensor{};
			const int64 n = gg.Numel();
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(gg.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)n, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("swiglu_fwd", NkString(kSwiGLUFwdNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gu), ob, p, (uint32)n);
			return NkTensorInternal::MakeGpu(gg.Shape(), gg.DType(), ob);
		}

		NkTensor NkGpuSwiGLUBackwardDu(const NkTensor &gate, const NkTensor &dh) {
			NkTensor gg = (gate.Device() == NkDevice::NK_GPU) ? gate : gate.ToGPU();
			NkTensor gh = (dh.Device() == NkDevice::NK_GPU) ? dh : dh.ToGPU();
			if (!gg.IsValid() || !gh.IsValid())
				return NkTensor{};
			const int64 n = gg.Numel();
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(gg.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)n, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("swiglu_bwd_du", NkString(kSwiGLUBwdDuNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gh), ob, p, (uint32)n);
			return NkTensorInternal::MakeGpu(gg.Shape(), gg.DType(), ob);
		}

		// `dhu` = dh ⊙ u, deja calcule par l'appelant (NkGpuMul).
		NkTensor NkGpuSwiGLUBackwardDg(const NkTensor &gate, const NkTensor &dhu) {
			NkTensor gg = (gate.Device() == NkDevice::NK_GPU) ? gate : gate.ToGPU();
			NkTensor gt = (dhu.Device() == NkDevice::NK_GPU) ? dhu : dhu.ToGPU();
			if (!gg.IsValid() || !gt.IsValid())
				return NkTensor{};
			const int64 n = gg.Numel();
			uint64 ob = NkTensorGpu::Get().CreateBuffer((nk_size)n * NkDTypeSize(gg.DType()));
			if (!ob)
				return NkTensor{};
			uint32 p[12] = {(uint32)n, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			NkTensorGpu::Get().RunOp3("swiglu_bwd_dg", NkString(kSwiGLUBwdDgNkSL), NkTensorInternal::GpuBuffer(gg),
									  NkTensorInternal::GpuBuffer(gt), ob, p, (uint32)n);
			return NkTensorInternal::MakeGpu(gg.Shape(), gg.DType(), ob);
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
			// ⚠️ CE SITE FAIT LES DEUX : une allocation ET un upload. Les postes n°1
			// (reserve de tampons) et n°2 (supprimer les uploads) de l'ordre de
			// bataille ne sont donc pas deux chantiers independants — chaque bascule
			// CPU->GPU compte une fois dans chacun.
			NkGpuAttribuerBascule(__builtin_return_address(0), (double)bytes);
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
