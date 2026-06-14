#pragma once
// =============================================================================
// NkComputeContext.h
// Couche compute RHI de haut niveau — calculs massifs, IA, simulations.
//
// NkComputeContext est la pièce manquante entre le RHI bas-niveau (Dispatch,
// BindComputePipeline) et les systèmes utilisateurs (NkMLContext, particules,
// physique, pathfinding, etc.).
//
// Ce qu'il apporte :
//   • Cache de pipelines compute (clé string → NkPipelineHandle)
//     Évite de recompiler le même shader depuis NkML, les particules ET la physique.
//   • Dispatch helpers dimensionnels (1D, 2D, 3D) avec calcul auto des groupes.
//   • Descriptor helpers : BindBuffer / BindTexture → flush automatique avant Dispatch.
//   • Push constants typés (template).
//   • Barrières UAV et transitions de layout simplifiées.
//   • Soumission asynchrone sur queue compute dédiée (Vulkan / DX12).
//   • Statistiques par passe (nb dispatches, éléments traités, temps GPU).
//
// Hiérarchie d'utilisation :
//
//   NkComputeContext       ← cette classe (couche intermédiaire)
//       ↑ utilise
//   NkIDevice + NkICommandBuffer   ← RHI bas-niveau
//       ↑ utilisé par
//   NkMLContext            ← intelligence artificielle / réseaux de neurones
//   NkParticleCompute      ← VFX GPU-driven
//   NkPhysicsGPU           ← simulation physique
//   NkPathfindingGPU       ← navigation massive
//
// Usage typique :
//
//   // Initialisation (une fois)
//   NkComputeContext compute;
//   compute.Init(device);
//
//   // Chaque frame :
//   auto* cmd = device->CreateCommandBuffer(NK_COMPUTE);
//   cmd->Begin();
//   compute.BeginPass(cmd, {"Update Particles"});
//     compute.SetPipeline(compute.GetOrCreatePipeline("ptcl_update", shaderHandle));
//     compute.BindBuffer(0, particleBuffer);  // binding=0 → SSBO
//     compute.BindBuffer(1, deadListBuffer);  // binding=1 → SSBO
//     compute.PushConstants(NkParticleParams{dt, gravity, count});
//     compute.Dispatch1D(count, 256);
//     compute.UAVBarrier(particleBuffer);
//   compute.EndPass();
//   cmd->End();
//   NkFenceHandle fence = compute.SubmitAsync(cmd);
//   // ... rendu graphics ...
//   compute.WaitForFence(fence); // si besoin de lire les résultats CPU
// =============================================================================

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // =========================================================================
    // NkComputePassStats — statistiques collectées pour une passe compute
    // =========================================================================
    struct NkComputePassStats {
        uint32  dispatchCount   = 0;  // nombre de Dispatch() émis dans la passe
        uint64  totalGroupsX    = 0;  // somme des groupsX sur tous les dispatches
        uint64  totalGroupsY    = 0;
        uint64  totalGroupsZ    = 0;
        float64 gpuMilliseconds = 0.0; // temps GPU mesuré (si enableTimestamp=true)
    };

    // =========================================================================
    // NkComputePipelineCacheEntry — entrée interne du cache de pipelines
    // =========================================================================
    struct NkComputePipelineCacheEntry {
        NkShaderHandle   shader;
        NkPipelineHandle pipeline;
    };

    // =========================================================================
    // NkBoundResource — binding en attente (flush avant le prochain Dispatch)
    // =========================================================================
    struct NkBoundResource {
        uint32           binding    = 0;
        NkDescriptorType type       = NkDescriptorType::NK_STORAGE_BUFFER;
        NkBufferHandle   buffer;
        NkTextureHandle  texture;
        NkSamplerHandle  sampler;
        uint64           bufOffset  = 0;
        uint64           bufRange   = 0;   // 0 = taille entière du buffer
    };

    // =========================================================================
    // NkComputeContext
    // =========================================================================
    class NkComputeContext {
        public:
            NkComputeContext()  = default;
            ~NkComputeContext() { if (mReady) Shutdown(); }

            // Non copiable — ressource unique (pipeline cache, descriptor sets).
            NkComputeContext(const NkComputeContext&)            = delete;
            NkComputeContext& operator=(const NkComputeContext&) = delete;

            // ── Cycle de vie ──────────────────────────────────────────────────
            bool Init   (NkIDevice* device);
            void Shutdown();
            bool IsReady() const { return mReady; }

            // ═════════════════════════════════════════════════════════════════
            // PASSE COMPUTE — délimite une région de travail
            // ═════════════════════════════════════════════════════════════════

            // Ouvre une passe compute sur le command buffer fourni.
            // Émet un debug marker (RenderDoc, PIX, Xcode) et, si
            // desc.enableTimestamp, écrit un timestamp GPU de début.
            // Précondition : cmd->Begin() a déjà été appelé.
            void BeginPass(NkICommandBuffer* cmd,
                            const NkComputePassDesc& desc = {});

            // Ferme la passe. Flush les bindings en attente si nécessaire,
            // écrit le timestamp GPU de fin, ferme le debug marker.
            void EndPass();

            bool              InPass() const { return mInPass; }
            NkICommandBuffer* GetCurrentCmd() const { return mCmd; }

            // ═════════════════════════════════════════════════════════════════
            // CACHE DE PIPELINES
            // ═════════════════════════════════════════════════════════════════

            // Retourne le pipeline associé à `key`, ou le compile/crée la
            // première fois. Idéal pour les pipelines partagés entre systèmes.
            // key       : identifiant unique (ex: "nkml_matmul", "ptcl_update")
            // shader    : handle vers le shader compute déjà compilé
            // debugName : affiché dans RenderDoc / PIX (null → key est utilisé)
            NkPipelineHandle GetOrCreatePipeline(const char*    key,
                                                  NkShaderHandle shader,
                                                  const char*    debugName = nullptr);

            // Variante : compile le shader GLSL inline et met en cache.
            // Compatible OpenGL 4.3+, Vulkan (via SPIRV-Cross si disponible).
            NkPipelineHandle GetOrCompileGLSL(const char* key,
                                               const char* glslSrc,
                                               const char* debugName = nullptr);

            // Retire un pipeline du cache et le détruit.
            void EvictPipeline(const char* key);

            // Vide tout le cache de pipelines.
            void ClearPipelineCache();

            // Lie un pipeline compute (doit être appelé à l'intérieur d'une passe).
            void SetPipeline(NkPipelineHandle pipeline);

            // ═════════════════════════════════════════════════════════════════
            // DESCRIPTORS — BINDING DE RESSOURCES
            // ═════════════════════════════════════════════════════════════════
            // Ces méthodes accumulent les bindings et les appliquent juste avant
            // le prochain Dispatch (lazy flush). Elles couvrent les cas usuels.
            // Pour les descriptor sets complexes, utilisez BindDescriptorSet().

            // Storage Buffer (SSBO / UAV) — lecture ET écriture.
            void BindBuffer(uint32 binding, NkBufferHandle buf,
                             uint64 offset = 0, uint64 range = 0);

            // Uniform Buffer — lecture seule, taille ≤ 64 Ko sur toutes les APIs.
            void BindUniformBuffer(uint32 binding, NkBufferHandle buf,
                                    uint64 offset = 0, uint64 range = 0);

            // Storage Texture (image load/store) — lecture ET écriture.
            void BindStorageTexture(uint32 binding, NkTextureHandle tex);

            // Sampled Texture — lecture seule avec sampler.
            void BindSampledTexture(uint32 binding, NkTextureHandle tex,
                                     NkSamplerHandle sampler);

            // Bind un descriptor set déjà construit (chemin avancé).
            // Annule et remplace les bindings accumulés par les méthodes ci-dessus.
            void BindDescriptorSet(NkDescSetHandle set, uint32 setIndex = 0,
                                    uint32* dynamicOffsets    = nullptr,
                                    uint32  dynamicOffsetCount = 0);

            // Remet à zéro tous les bindings accumulés sans les appliquer.
            void ClearBindings();

            // ═════════════════════════════════════════════════════════════════
            // PUSH CONSTANTS
            // ═════════════════════════════════════════════════════════════════

            // Envoie des push constants typés au shader compute courant.
            // Préférer cette surcharge : elle garantit la taille correcte.
            template<typename T>
            void PushConstants(const T& data, uint32 offset = 0) {
                PushConstantsRaw(&data, static_cast<uint32>(sizeof(T)), offset);
            }

            // Envoie des push constants bruts (taille explicite).
            void PushConstantsRaw(const void* data, uint32 size, uint32 offset = 0);

            // ═════════════════════════════════════════════════════════════════
            // DISPATCH
            // ═════════════════════════════════════════════════════════════════
            // Tous les Dispatch* flushent automatiquement les bindings en attente.

            // Dispatch explicite en nombre de groupes.
            void Dispatch(uint32 groupsX, uint32 groupsY = 1, uint32 groupsZ = 1);

            // 1D — couvre `count` éléments avec des groupes de `groupSize` threads.
            // groupsX = ceil(count / groupSize)
            // Idéal : buffers linéaires (particules, IA, pathfinding, etc.)
            void Dispatch1D(uint32 count, uint32 groupSize = 256);

            // 2D — couvre une image W×H avec des tuiles tileX×tileY.
            // groupsX = ceil(W / tileX), groupsY = ceil(H / tileY)
            // Idéal : post-processing, génération de heightmaps, convolutions
            void Dispatch2D(uint32 width, uint32 height,
                             uint32 tileX = 16, uint32 tileY = 16);

            // 3D — couvre un volume W×H×D avec des tuiles 3D.
            // Idéal : simulation de fluides, voxels, navigation 3D
            void Dispatch3D(uint32 width, uint32 height, uint32 depth,
                             uint32 tileX = 8, uint32 tileY = 8, uint32 tileZ = 8);

            // Dispatch indirect — les dimensions de dispatch sont dans un buffer GPU.
            // Idéal : indirect rendering, stream compaction, sort GPU
            void DispatchIndirect(NkBufferHandle argsBuffer, uint64 offset = 0);

            // ═════════════════════════════════════════════════════════════════
            // BARRIÈRES & SYNCHRONISATION
            // ═════════════════════════════════════════════════════════════════
            // À appeler entre deux dispatches qui lisent/écrivent les mêmes ressources.
            // Oubli d'une barrière → données corrompues ou race condition GPU.

            // UAV barrier sur un buffer — après écriture, avant lecture du même buffer.
            void UAVBarrier(NkBufferHandle buf);

            // UAV barrier sur une texture — après image store, avant image load/sample.
            void UAVBarrier(NkTextureHandle tex);

            // Transition d'une texture vers UNORDERED_ACCESS (avant écriture compute).
            // Typiquement : texture venant d'être lue en graphics → écriture compute.
            void TransitionForCompute(NkTextureHandle tex,
                                       NkResourceState before = NkResourceState::NK_SHADER_READ);

            // Transition d'une texture vers SHADER_READ (après écriture compute).
            // Typiquement : résultat d'un compute → lu en fragment shader.
            void TransitionForGraphics(NkTextureHandle tex,
                                        NkResourceState after = NkResourceState::NK_SHADER_READ);

            // Barrière globale tous-stages → tous-stages.
            // Très coûteuse — à réserver aux cas où les dépendances sont inconnues.
            void FullBarrier();

            // ═════════════════════════════════════════════════════════════════
            // SOUMISSION ASYNCHRONE
            // ═════════════════════════════════════════════════════════════════
            // Sur Vulkan et DX12, soumet sur la queue compute dédiée.
            // Sur OpenGL / DX11, soumet sur la queue principale (synchrone).
            //
            // Le compute et le rendu graphics s'exécutent alors en PARALLÈLE sur
            // le GPU (sur les architectures qui ont une queue compute séparée).
            // Exemple : NkML tourne pendant que la scène 3D est rendue.

            // Soumet un command buffer compute de façon asynchrone.
            // waitSemaphore   : attend que le graphics soit arrivé à ce point
            // signalSemaphore : signale quand le compute est terminé (pour le graphics)
            // Retourne une fence que l'on peut attendre côté CPU si nécessaire.
            NkFenceHandle SubmitAsync(NkICommandBuffer*  cmd,
                                       NkSemaphoreHandle  waitSemaphore   = {},
                                       NkSemaphoreHandle  signalSemaphore = {});

            // Attend côté CPU que la fence soit signalée.
            // timeoutNanos = UINT64_MAX → attente infinie.
            void WaitForFence(NkFenceHandle fence,
                               uint64 timeoutNanos = UINT64_MAX);

            // Sync queue-to-queue : garantit que le compute est terminé avant
            // que le command buffer graphics `graphicsCmd` lise ses résultats.
            // Utiliser les semaphores pour la vraie synchronisation GPU-GPU.
            void SyncComputeToGraphics(NkSemaphoreHandle computeSignal,
                                        NkICommandBuffer* graphicsCmd);

            // ═════════════════════════════════════════════════════════════════
            // CAPACITÉS & LIMITES
            // ═════════════════════════════════════════════════════════════════

            bool   SupportsAsyncCompute()    const;  // queue compute dédiée ?
            bool   SupportsIndirectDispatch() const; // DispatchIndirect disponible ?
            uint32 MaxGroupSizeX()           const;
            uint32 MaxGroupSizeY()           const;
            uint32 MaxGroupSizeZ()           const;
            uint32 MaxSharedMemoryBytes()    const;

            // Retourne la taille optimale de groupe pour les dispatches 1D.
            // Prend en compte les capacités du device (warp/wavefront size).
            uint32 OptimalGroupSize1D() const;

            // ═════════════════════════════════════════════════════════════════
            // STATISTIQUES
            // ═════════════════════════════════════════════════════════════════

            const NkComputePassStats& GetLastPassStats()    const { return mLastStats; }
            void                      ResetPassStats()             { mLastStats = {}; }

        private:
            // Applique les bindings accumulés avant un Dispatch.
            // Crée/met à jour le descriptor set de travail si nécessaire.
            void FlushBindings();

            // Met à jour mCurrentStats avec le dispatch en cours.
            void TrackDispatch(uint32 gx, uint32 gy, uint32 gz);

            // ── État interne ──────────────────────────────────────────────────

            NkIDevice*        mDevice  = nullptr;
            NkICommandBuffer* mCmd     = nullptr;
            bool              mReady   = false;
            bool              mInPass  = false;

            NkPipelineHandle  mBoundPipeline;          // pipeline courant
            bool              mPipelineBound = false;

            // Cache de pipelines compute (partagé avec NkML, particules, physique...)
            NkUnorderedMap<NkString, NkComputePipelineCacheEntry> mPipelineCache;

            // Bindings accumulés (flush lazy avant Dispatch)
            NkVector<NkBoundResource> mPendingBindings;
            bool                      mBindingsDirty = false;

            // Descriptor set de travail (géré en interne, reconstruit si dirty)
            NkDescSetHandle   mWorkingSet;
            NkDescSetHandle   mWorkingLayout;
            bool              mWorkingSetValid = false;

            // Statistiques de la passe courante
            NkComputePassStats mCurrentStats;
            NkComputePassStats mLastStats;

            // Timestamp GPU (index dans le query pool)
            uint32 mTimestampBeginIdx = UINT32_MAX;
            uint32 mTimestampEndIdx   = UINT32_MAX;
            bool   mTimestampEnabled  = false;
    };

    // =========================================================================
    // Helpers statiques — calcul des dimensions de dispatch
    // =========================================================================
    // ceil(n / groupSize) — sans débordement entier
    inline uint32 NkComputeGroups(uint32 count, uint32 groupSize) {
        return (count + groupSize - 1) / groupSize;
    }

    // Retourne le multiple de `alignment` supérieur ou égal à `value`.
    // Utile pour aligner les tailles de buffer avant d'écrire depuis compute.
    inline uint32 NkComputeAlignUp(uint32 value, uint32 alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // =========================================================================
    // NkComputeBuilder — API fluide pour construire un dispatch en une expression
    // =========================================================================
    // Usage :
    //   NkComputeBuilder(ctx)
    //     .Pipeline(pipeHandle)
    //     .Buffer(0, inputBuf)
    //     .Buffer(1, outputBuf)
    //     .Push(myParams)
    //     .Dispatch1D(count, 256);
    // =========================================================================
    class NkComputeBuilder {
        public:
            explicit NkComputeBuilder(NkComputeContext& ctx) : mCtx(ctx) {}

            NkComputeBuilder& Pipeline(NkPipelineHandle pipe) {
                mCtx.SetPipeline(pipe);
                return *this;
            }

            NkComputeBuilder& Buffer(uint32 binding, NkBufferHandle buf,
                                      uint64 offset = 0, uint64 range = 0) {
                mCtx.BindBuffer(binding, buf, offset, range);
                return *this;
            }

            NkComputeBuilder& Uniform(uint32 binding, NkBufferHandle buf,
                                       uint64 offset = 0, uint64 range = 0) {
                mCtx.BindUniformBuffer(binding, buf, offset, range);
                return *this;
            }

            NkComputeBuilder& StorageTex(uint32 binding, NkTextureHandle tex) {
                mCtx.BindStorageTexture(binding, tex);
                return *this;
            }

            NkComputeBuilder& SampledTex(uint32 binding, NkTextureHandle tex,
                                          NkSamplerHandle sampler) {
                mCtx.BindSampledTexture(binding, tex, sampler);
                return *this;
            }

            template<typename T>
            NkComputeBuilder& Push(const T& data, uint32 offset = 0) {
                mCtx.PushConstants(data, offset);
                return *this;
            }

            // Terminateurs — émettent le Dispatch et retournent le builder pour
            // éventuellement chaîner un second dispatch (avec UAVBarrier entre).
            NkComputeBuilder& Dispatch(uint32 gx, uint32 gy = 1, uint32 gz = 1) {
                mCtx.Dispatch(gx, gy, gz);
                return *this;
            }

            NkComputeBuilder& Dispatch1D(uint32 count, uint32 groupSize = 256) {
                mCtx.Dispatch1D(count, groupSize);
                return *this;
            }

            NkComputeBuilder& Dispatch2D(uint32 w, uint32 h,
                                          uint32 tx = 16, uint32 ty = 16) {
                mCtx.Dispatch2D(w, h, tx, ty);
                return *this;
            }

            NkComputeBuilder& Dispatch3D(uint32 w, uint32 h, uint32 d,
                                          uint32 tx = 8, uint32 ty = 8, uint32 tz = 8) {
                mCtx.Dispatch3D(w, h, d, tx, ty, tz);
                return *this;
            }

            NkComputeBuilder& UAVBarrier(NkBufferHandle buf) {
                mCtx.UAVBarrier(buf);
                return *this;
            }

            NkComputeBuilder& UAVBarrier(NkTextureHandle tex) {
                mCtx.UAVBarrier(tex);
                return *this;
            }

        private:
            NkComputeContext& mCtx;
    };

} // namespace nkentseu
