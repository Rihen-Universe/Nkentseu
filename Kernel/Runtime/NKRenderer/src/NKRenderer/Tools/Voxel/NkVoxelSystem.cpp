#include "pch.h"
// =============================================================================
// NkVoxelSystem.cpp  — NKRenderer v5.0  (Tools/Voxel/)
//
// Couche GPU completee via NkComputeContext (BeginPass/SetPipeline/
// BindStorageTexture/PushConstants/Dispatch/UAVBarrier/EndPass). Le dispatch
// d'edition est borne a la DIRTY BOX (cout proportionnel au volume edite).
//
// ⚠️ Depend de la creation reelle des textures 3D (NkVoxelVolume::CreateTargets,
//    encore TODO) : tant que le volume n'est pas valide, mReady reste false et
//    les passes ne s'enregistrent pas. La passe Raymarch a besoin des cibles
//    G-buffer (integration NkDeferredPass) pour ecrire son resultat.
// =============================================================================
#include "NKRenderer/Tools/Voxel/NkVoxelSystem.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Core/NkRenderGraph.h"

namespace nkentseu {
    namespace renderer {

        NkVoxelSystem::~NkVoxelSystem() noexcept { Shutdown(); }

        bool NkVoxelSystem::Init(NkIDevice* device, NkRenderGraph* graph,
                                 NkTextureLibrary* texLib, NkShaderLibrary* shaderLib,
                                 const NkVoxelConfig& cfg) noexcept {
            mDevice = device; mGraph = graph; mTexLib = texLib; mShaders = shaderLib;
            mCfg = cfg;
            if (!mVolume.Init(device, cfg)) { /* CreateTargets TODO -> false pour l'instant */ }
            if (!mCompute.Init(device))     return false;
            if (!mPipelines.Init(&mCompute)) return false;
            // Pret seulement quand le volume est reellement alloue.
            mReady = mVolume.IsValid() && mCompute.IsReady() && mPipelines.IsValid();
            return mReady;
        }

        void NkVoxelSystem::Shutdown() noexcept {
            mPipelines.Shutdown();
            mCompute.Shutdown();
            mVolume.Shutdown();
            mReady  = false;
            mDevice = nullptr;
            mGraph  = nullptr;
        }

        void NkVoxelSystem::SetCamera(const NkMat4f& invViewProj, const NkVec3f& camPosWorld) noexcept {
            mInvViewProj = invViewProj;
            mCamPos      = camPosWorld;
        }

        NkVec3f NkVoxelSystem::WorldToVoxel(const NkVec3f& w) const noexcept {
            const float32 inv = (mCfg.voxelSize != 0.f) ? (1.f / mCfg.voxelSize) : 0.f;
            return NkVec3f{ (w.x - mCfg.originWorld.x) * inv,
                            (w.y - mCfg.originWorld.y) * inv,
                            (w.z - mCfg.originWorld.z) * inv };
        }

        NkVec3f NkVoxelSystem::VoxelToWorld(const NkVec3f& v) const noexcept {
            return NkVec3f{ mCfg.originWorld.x + v.x * mCfg.voxelSize,
                            mCfg.originWorld.y + v.y * mCfg.voxelSize,
                            mCfg.originWorld.z + v.z * mCfg.voxelSize };
        }

        void NkVoxelSystem::RegisterToRenderGraph() noexcept {
            if (!mReady || !mGraph) return;
            mVolume.ImportToGraph(mGraph);

            // Passe 1 : Edit (mute le volume). Pilotee par l'input -> alwaysExecute.
            NkPassBuilder& edit = mGraph->AddComputePass("Voxel_Edit");
            edit.WritesStorage(mVolume.ResDensity())
                .WritesStorage(mVolume.ResColor())
                .SetAlwaysExecute(true)
                .Execute([this](NkICommandBuffer* cmd) { RecordEditPass(cmd); });

            // Passe 2 : Raymarch (volume -> G-buffer deferred).
            if (mCfg.resolveToGBuffer) {
                NkPassBuilder& rm = mGraph->AddComputePass("Voxel_Raymarch");
                rm.Reads(mVolume.ResDensity(), mVolume.ResColor())
                  .SetAlwaysExecute(true)
                  .Execute([this](NkICommandBuffer* cmd) { RecordRaymarchPass(cmd); });
            }
            // Barriers inserees automatiquement par le graph entre les passes.
        }

        void NkVoxelSystem::SetBrush(const NkVoxelBrush& brush) noexcept { mBrush = brush; }

        void NkVoxelSystem::BeginStrokeWorld(const NkVec3f& posWorld, float32 pressure) noexcept {
            mStroke.Begin(mBrush);
            mStroke.AddSample(WorldToVoxel(posWorld), pressure);
        }
        void NkVoxelSystem::AddStrokeSampleWorld(const NkVec3f& posWorld, float32 pressure) noexcept {
            mStroke.AddSample(WorldToVoxel(posWorld), pressure);
        }
        void NkVoxelSystem::EndStroke() noexcept { mStroke.End(); }

        void NkVoxelSystem::ClearVolume() noexcept {
            // TODO(Voxel): poser un flag consomme au prochain RecordEditPass
            //   (qui appellerait mVolume.Clear(cmd) avant les dabs).
        }

        // ─────────────────────────────────────────────────────────────────────
        // Edit : dispatch des dabs en attente, borne a la dirty box.
        // ─────────────────────────────────────────────────────────────────────
        void NkVoxelSystem::RecordEditPass(NkICommandBuffer* cmd) noexcept {
            mStats.Reset();
            if (!mReady || !cmd) return;
            if (mStroke.PendingDabs().Empty()) return;

            // Clampe la dirty box aux dimensions de la grille.
            NkVoxelBox box = mStroke.DirtyBox();
            if (box.maxX > (int32)mCfg.dimX) box.maxX = (int32)mCfg.dimX;
            if (box.maxY > (int32)mCfg.dimY) box.maxY = (int32)mCfg.dimY;
            if (box.maxZ > (int32)mCfg.dimZ) box.maxZ = (int32)mCfg.dimZ;
            if (box.IsEmpty()) { mStroke.ClearPending(); return; }

            NkPipelineHandle pipe = mPipelines.Edit();
            if (!pipe.IsValid()) return;

            const uint32 ts = kNkVoxelTileSize;
            const uint32 gx = ((uint32)box.Width()  + ts - 1) / ts;
            const uint32 gy = ((uint32)box.Height() + ts - 1) / ts;
            const uint32 gz = ((uint32)box.Depth()  + ts - 1) / ts;

            NkComputePassDesc pass; pass.debugName = "Voxel_Edit";
            mCompute.BeginPass(cmd, pass);
            mCompute.SetPipeline(pipe);
            mCompute.BindStorageTexture(0, mVolume.Density()); // image3D r16f
            mCompute.BindStorageTexture(1, mVolume.Color());   // image3D rgba8

            const auto& dabs = mStroke.PendingDabs();
            for (uint32 i = 0; i < (uint32)dabs.Size(); ++i) {
                NkVoxelBrushGPU pc = MakeVoxelGPU(mStroke.Brush(), dabs[i],
                                                  box.minX, box.minY, box.minZ);
                mCompute.PushConstants(pc);
                mCompute.Dispatch(gx, gy, gz);           // borne a la dirty box
                mStats.bricksDispatched += gx * gy * gz;
                // Le dab suivant peut lire ce que celui-ci vient d'ecrire.
                mCompute.UAVBarrier(mVolume.Density());
                mCompute.UAVBarrier(mVolume.Color());
            }
            mStats.dabsDispatched = (uint32)dabs.Size();
            mCompute.EndPass();
            mStroke.ClearPending();
        }

        // ─────────────────────────────────────────────────────────────────────
        // Raymarch : volume -> G-buffer. Le bind des cibles G-buffer + la taille
        // ecran dependent de l'integration NkDeferredPass (handles a fournir).
        // ─────────────────────────────────────────────────────────────────────
        void NkVoxelSystem::RecordRaymarchPass(NkICommandBuffer* cmd) noexcept {
            if (!mReady || !cmd) return;
            NkPipelineHandle pipe = mPipelines.Raymarch();
            if (!pipe.IsValid()) return;

            NkComputePassDesc pass; pass.debugName = "Voxel_Raymarch";
            mCompute.BeginPass(cmd, pass);
            mCompute.SetPipeline(pipe);
            mCompute.BindStorageTexture(0, mVolume.Density());
            mCompute.BindStorageTexture(1, mVolume.Color());
            // TODO(Voxel): BindStorageTexture(2/3, G-buffer albedo/normal) + push
            //   (invViewProj/camPos/origin/voxelSize/dims), puis :
            //   mCompute.Dispatch2D(screenW, screenH, 8, 8);
            mCompute.EndPass();
        }

    } // namespace renderer
} // namespace nkentseu
