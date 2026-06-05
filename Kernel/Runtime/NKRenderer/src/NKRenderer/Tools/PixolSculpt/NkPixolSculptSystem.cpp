#include "pch.h"
// =============================================================================
// NkPixolSculptSystem.cpp  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Couche GPU completee via NkComputeContext. Le dispatch de brosse est borne au
// DIRTY RECT (cout constant en resolution). Miroir 2D de NkVoxelSystem.
//
// ⚠️ Depend de la creation reelle des storage images (NkPixolBuffer::CreateTargets,
//    desormais implementee). La passe Resolve a besoin des cibles G-buffer
//    (integration NkDeferredPass) pour ecrire son resultat.
// =============================================================================
#include "NKRenderer/Tools/PixolSculpt/NkPixolSculptSystem.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Core/NkRenderGraph.h"

namespace nkentseu {
    namespace renderer {

        NkPixolSculptSystem::~NkPixolSculptSystem() noexcept { Shutdown(); }

        bool NkPixolSculptSystem::Init(NkIDevice* device, NkRenderGraph* graph,
                                       NkTextureLibrary* texLib, NkShaderLibrary* shaderLib,
                                       uint32 width, uint32 height,
                                       const NkPixolSculptConfig& cfg) noexcept {
            mDevice = device; mGraph = graph; mTexLib = texLib; mShaders = shaderLib;
            mCfg    = cfg;
            mWidth  = (cfg.width  != 0) ? cfg.width  : width;
            mHeight = (cfg.height != 0) ? cfg.height : height;

            if (!mPixol.Init(device, mWidth, mHeight, mCfg)) { /* depend des textures */ }
            if (!mCompute.Init(device))      return false;
            if (!mPipelines.Init(&mCompute)) return false;
            mReady = mPixol.IsValid() && mCompute.IsReady() && mPipelines.IsValid();
            return mReady;
        }

        void NkPixolSculptSystem::Shutdown() noexcept {
            mPipelines.Shutdown();
            mCompute.Shutdown();
            mPixol.Shutdown();
            mReady  = false;
            mDevice = nullptr;
            mGraph  = nullptr;
        }

        bool NkPixolSculptSystem::Resize(uint32 width, uint32 height) noexcept {
            mWidth = width; mHeight = height;
            return mPixol.Resize(width, height);
        }

        void NkPixolSculptSystem::RegisterToRenderGraph() noexcept {
            if (!mReady || !mGraph) return;
            mPixol.ImportToGraph(mGraph);

            NkPassBuilder& brush = mGraph->AddComputePass("PixolSculpt_Brush");
            brush.WritesStorage(mPixol.ResDepth())
                 .WritesStorage(mPixol.ResNormal())
                 .WritesStorage(mPixol.ResColor())
                 .SetAlwaysExecute(true)
                 .Execute([this](NkICommandBuffer* cmd) { RecordBrushPass(cmd); });

            if (mCfg.resolveToGBuffer) {
                NkPassBuilder& res = mGraph->AddComputePass("PixolSculpt_Resolve");
                res.Reads(mPixol.ResDepth(), mPixol.ResNormal())
                   .Reads(mPixol.ResColor())
                   .SetAlwaysExecute(true)
                   .Execute([this](NkICommandBuffer* cmd) { RecordResolvePass(cmd); });
            }
        }

        void NkPixolSculptSystem::SetBrush(const NkSculptBrush& brush) noexcept { mBrush = brush; }

        void NkPixolSculptSystem::BeginStroke(const NkVec2f& screenPos, float32 pressure) noexcept {
            mStroke.Begin(mBrush);
            mStroke.AddSample(screenPos, pressure);
        }
        void NkPixolSculptSystem::AddStrokeSample(const NkVec2f& screenPos, float32 pressure) noexcept {
            mStroke.AddSample(screenPos, pressure);
        }
        void NkPixolSculptSystem::EndStroke() noexcept { mStroke.End(); }

        void NkPixolSculptSystem::ClearCanvas() noexcept {
            // TODO(PixolSculpt): flag consomme au prochain RecordBrushPass (mPixol.Clear).
        }

        void NkPixolSculptSystem::RecordBrushPass(NkICommandBuffer* cmd) noexcept {
            mStats.Reset();
            if (!mReady || !cmd) return;
            if (mStroke.PendingDabs().Empty()) return;

            NkSculptRect dirty = mStroke.DirtyRect();
            // Clampe le rect a la resolution.
            if (dirty.x + dirty.w > (int32)mWidth)  dirty.w = (int32)mWidth  - dirty.x;
            if (dirty.y + dirty.h > (int32)mHeight) dirty.h = (int32)mHeight - dirty.y;
            if (dirty.IsEmpty()) { mStroke.ClearPending(); return; }

            NkPipelineHandle pipe = mPipelines.Brush();
            if (!pipe.IsValid()) return;

            const uint32 ts = mCfg.tileSize ? mCfg.tileSize : kNkSculptTileSize;
            const uint32 gx = ((uint32)dirty.w + ts - 1) / ts;
            const uint32 gy = ((uint32)dirty.h + ts - 1) / ts;

            NkComputePassDesc pass; pass.debugName = "PixolSculpt_Brush";
            mCompute.BeginPass(cmd, pass);
            mCompute.SetPipeline(pipe);
            mCompute.BindStorageTexture(0, mPixol.Depth());
            mCompute.BindStorageTexture(1, mPixol.Normal());
            mCompute.BindStorageTexture(2, mPixol.Color());

            const auto& dabs = mStroke.PendingDabs();
            for (uint32 i = 0; i < (uint32)dabs.Size(); ++i) {
                NkSculptBrushGPU pc = MakeBrushGPU(mStroke.Brush(), dabs[i], dirty.x, dirty.y);
                mCompute.PushConstants(pc);
                mCompute.Dispatch(gx, gy, 1);          // borne au dirty rect
                mStats.tilesDispatched += gx * gy;
                mCompute.UAVBarrier(mPixol.Depth());
                mCompute.UAVBarrier(mPixol.Color());
            }
            mStats.dabsDispatched = (uint32)dabs.Size();
            mCompute.EndPass();
            mStroke.ClearPending();
        }

        void NkPixolSculptSystem::RecordResolvePass(NkICommandBuffer* cmd) noexcept {
            if (!mReady || !cmd) return;
            NkPipelineHandle pipe = mPipelines.Resolve();
            if (!pipe.IsValid()) return;

            NkComputePassDesc pass; pass.debugName = "PixolSculpt_Resolve";
            mCompute.BeginPass(cmd, pass);
            mCompute.SetPipeline(pipe);
            mCompute.BindStorageTexture(0, mPixol.Depth());
            mCompute.BindStorageTexture(1, mPixol.Normal());
            mCompute.BindStorageTexture(2, mPixol.Color());
            // TODO(PixolSculpt): BindStorageTexture(3/4, G-buffer albedo/normal) puis
            //   mCompute.Dispatch2D(mWidth, mHeight, mCfg.tileSize, mCfg.tileSize);
            mCompute.EndPass();
        }

    } // namespace renderer
} // namespace nkentseu
