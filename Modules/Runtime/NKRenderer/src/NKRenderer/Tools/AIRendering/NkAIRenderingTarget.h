#pragma once
// =============================================================================
// NkAIRenderingTarget.h  — NKRenderer v4.0  (Tools/AIRendering/)
//
// Cible de rendu pour pipelines d'inférence IA (upscaling, super-resolution,
// segmentation sémantique, débruitage neural, etc.).
//
// Fonctionnalités :
//   • Readback GPU → CPU asynchrone (non-bloquant, ring buffer N frames)
//   • Conversion de format GPU → format CPU pour inférence (fp16, fp32, uint8)
//   • Callback à la réception des données (frame latency configurable)
//   • Plusieurs canaux (couleur, profondeur, normal, segmentation, custom)
//   • Mode synchrone pour tests / offline rendering
//
// Usage :
//   NkAIRenderingTarget* t = sys.Create({
//       .width=1920, .height=1080,
//       .channels=NkAIChannel::NK_AI_COLOR | NkAIChannel::NK_AI_DEPTH,
//       .format=NkAIDataFormat::NK_AI_FP32,
//       .ringSize=3,
//       .callback=[](const NkAIFrame& f){ runInference(f); }
//   });
//   // par frame
//   t->Capture(cmd, colorTex, depthTex);
//   sys.FlushReadbacks();   // déclenchement des callbacks prêts
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Canaux de données capturés
    // =========================================================================
    enum class NkAIChannel : uint32 {
        NK_AI_NONE        = 0,
        NK_AI_COLOR       = 1 << 0,  // RGBA rendu
        NK_AI_DEPTH       = 1 << 1,  // profondeur linéarisée
        NK_AI_NORMAL      = 1 << 2,  // normales world-space
        NK_AI_ALBEDO      = 1 << 3,  // albedo G-buffer
        NK_AI_MOTION      = 1 << 4,  // motion vectors
        NK_AI_INSTANCE_ID = 1 << 5,  // ID instance (segmentation)
        NK_AI_CUSTOM      = 1 << 6,  // texture user-définie
    };
    inline NkAIChannel operator|(NkAIChannel a, NkAIChannel b)
        { return (NkAIChannel)((uint32)a | (uint32)b); }
    inline bool NkAIHas(NkAIChannel mask, NkAIChannel flag)
        { return ((uint32)mask & (uint32)flag) != 0; }

    // =========================================================================
    // Format des données CPU après readback
    // =========================================================================
    enum class NkAIDataFormat : uint8 {
        NK_AI_UINT8   = 0,  // [0,255] normalisé — inference légère
        NK_AI_FP16    = 1,  // float16 — inférence GPU (ONNX, TensorRT)
        NK_AI_FP32    = 2,  // float32 — précision maximale / OIDN
    };

    // =========================================================================
    // Frame de données prête pour l'inférence
    // =========================================================================
    struct NkAIFrame {
        uint64      frameIndex  = 0;
        uint32      width       = 0;
        uint32      height      = 0;
        NkAIChannel channels    = NkAIChannel::NK_AI_NONE;
        NkAIDataFormat format   = NkAIDataFormat::NK_AI_FP32;
        float32     timestamp   = 0.f;

        // Pointeurs vers les buffers CPU (durée de vie limitée au callback)
        const void* colorData     = nullptr;
        const void* depthData     = nullptr;
        const void* normalData    = nullptr;
        const void* albedoData    = nullptr;
        const void* motionData    = nullptr;
        const void* instanceIdData= nullptr;
        const void* customData    = nullptr;

        uint32 strideBytes = 0; // stride par ligne en bytes
    };

    // =========================================================================
    // Callback déclenché quand une frame est prête côté CPU
    // =========================================================================
    using NkAIFrameCallback = NkFunction<void(const NkAIFrame&)>;

    // =========================================================================
    // Descripteur de création
    // =========================================================================
    struct NkAITargetDesc {
        uint32          width    = 1920;
        uint32          height   = 1080;
        NkAIChannel     channels = NkAIChannel::NK_AI_COLOR;
        NkAIDataFormat  format   = NkAIDataFormat::NK_AI_FP32;
        uint32          ringSize = 3;         // frames en vol simultanément
        bool            sync     = false;     // bloquant (mode debug/offline)
        NkAIFrameCallback callback;
        NkString         name;
        NkTexHandle      customTex;           // si NK_AI_CUSTOM
    };

    // =========================================================================
    // NkAIRenderingTarget — une cible unique
    // =========================================================================
    class NkAIRenderingTarget {
    public:
        explicit NkAIRenderingTarget(const NkAITargetDesc& desc);
        ~NkAIRenderingTarget();

        // ── Capture (appelé chaque frame voulue) ─────────────────────────────
        void Capture(NkICommandBuffer* cmd,
                     NkTexHandle colorTex,
                     NkTexHandle depthTex     = NkTexHandle::Null(),
                     NkTexHandle normalTex    = NkTexHandle::Null(),
                     NkTexHandle albedoTex    = NkTexHandle::Null(),
                     NkTexHandle motionTex    = NkTexHandle::Null(),
                     NkTexHandle instanceTex  = NkTexHandle::Null());

        // Flush les readbacks prêts et déclenche les callbacks
        void Flush();

        // Attendre que tous les readbacks soient terminés (sync)
        void WaitAll();

        // ── Config dynamique ──────────────────────────────────────────────────
        void SetCallback(NkAIFrameCallback cb);
        void Enable (bool e);
        bool IsEnabled() const { return mEnabled; }

        // ── Accès ─────────────────────────────────────────────────────────────
        const NkAITargetDesc& GetDesc()       const { return mDesc; }
        uint64                GetFrameCount() const { return mCaptureCount; }

    private:
        friend class NkAIRenderingSystem;

        NkAITargetDesc mDesc;
        bool           mEnabled      = true;
        uint64         mCaptureCount = 0;

        struct RingSlot {
            NkBufferHandle stagingBuf;  // buffer readback CPU-visible
            uint64         frameIdx  = 0;
            bool           pending   = false;
            float32        timestamp = 0.f;
            // mapped pointers par canal
            void* ptrs[7] = {};
        };

        NkVector<RingSlot> mRing;
        uint32             mWriteSlot = 0;
        uint32             mReadSlot  = 0;

        void IssueCopy(NkICommandBuffer* cmd, NkTexHandle src,
                        RingSlot& slot, uint32 chanIdx);
        void TriggerCallback(const RingSlot& slot);
    };

    // =========================================================================
    // NkAIRenderingSystem — gestionnaire de toutes les cibles IA
    // =========================================================================
    class NkAIRenderingSystem {
    public:
        NkAIRenderingSystem()  = default;
        ~NkAIRenderingSystem();

        bool Init(NkIDevice* device);
        void Shutdown();

        // ── Gestion des cibles ────────────────────────────────────────────────
        NkAIRenderingTarget* Create (const NkAITargetDesc& desc);
        void                 Destroy(NkAIRenderingTarget*& target);

        // ── Flush global (appeler en fin de frame, après Submit) ───────────────
        void FlushReadbacks();

        // ── Capacités ─────────────────────────────────────────────────────────
        bool IsAsyncReadbackSupported() const { return mAsyncSupported; }
        uint32 GetTargetCount() const { return (uint32)mTargets.Size(); }

    private:
        NkIDevice*                       mDevice        = nullptr;
        bool                             mAsyncSupported= false;
        bool                             mReady         = false;
        NkVector<NkAIRenderingTarget*>   mTargets;
    };

} // namespace renderer
} // namespace nkentseu
