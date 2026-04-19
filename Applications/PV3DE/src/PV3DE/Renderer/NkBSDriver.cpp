#include "NkBSDriver.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
    namespace pv3de {

        bool NkBSDriver::Init(NkIDevice* device, nk_uint32 count) noexcept {
            mDevice = device;
            mCount  = NkMin(count, kMaxBlendshapes);
            memset(mWeightsCPU, 0, sizeof(mWeightsCPU));

            if (!mDevice) return false;

            // Créer un buffer GPU pour les poids
            // Taille : kMaxBlendshapes * sizeof(float) aligné sur 16 bytes (std140)
            NkBufferDesc bd;
            bd.size      = kMaxBlendshapes * sizeof(nk_float32);
            bd.usage     = NkBufferUsage::NK_UNIFORM_BUFFER;
            bd.cpuAccess = NkCPUAccess::NK_CPU_WRITE;
            bd.debugName = "BlendshapeWeights";
            mGPUBuffer   = mDevice->CreateBuffer(bd);

            if (!mGPUBuffer.IsValid()) {
                logger.Errorf("[NkBSDriver] CreateBuffer échoué\n");
                return false;
            }

            logger.Infof("[NkBSDriver] Init — {} blendshapes\n", mCount);
            return true;
        }

        void NkBSDriver::Shutdown() noexcept {
            if (mDevice && mGPUBuffer.IsValid()) {
                mDevice->DestroyBuffer(mGPUBuffer);
                mGPUBuffer = {};
            }
        }

        void NkBSDriver::SetWeights(const nk_float32* weights, nk_uint32 count) noexcept {
            nk_uint32 n = NkMin(count, mCount);
            if (memcmp(mWeightsCPU, weights, n * sizeof(nk_float32)) == 0) return;
            memcpy(mWeightsCPU, weights, n * sizeof(nk_float32));
            // Remettre à zéro les poids restants
            if (n < kMaxBlendshapes)
                memset(mWeightsCPU + n, 0, (kMaxBlendshapes - n) * sizeof(nk_float32));
            mDirty = true;
        }

        void NkBSDriver::Flush(NkICommandBuffer* cmd) noexcept {
            if (!mDirty || !mDevice || !mGPUBuffer.IsValid()) return;
            (void)cmd;
            // Upload CPU → GPU via Map/Unmap
            void* ptr = mDevice->MapBuffer(mGPUBuffer);
            if (ptr) {
                memcpy(ptr, mWeightsCPU, kMaxBlendshapes * sizeof(nk_float32));
                mDevice->UnmapBuffer(mGPUBuffer);
            }
            mDirty = false;
        }

        void NkBSDriver::Bind(NkICommandBuffer* cmd) noexcept {
            if (!mGPUBuffer.IsValid()) return;
            cmd->BindUniformBuffer(mGPUBuffer, kBindingSlot);
        }

        void NkBSDriver::Unbind(NkICommandBuffer* cmd) noexcept {
            (void)cmd;
            // La plupart des backends ne nécessitent pas un unbind explicite
        }

    } // namespace pv3de
} // namespace nkentseu
