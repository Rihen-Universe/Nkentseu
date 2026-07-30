#include "NkBSDriver.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
	namespace pv3de {

		using namespace nkentseu::math;

		// =====================================================================
		// STUB Phase R1 (2026-07-25) — cf. ROADMAP.md "Rendu 3D". L'ancien code
		// appelait NkBufferDesc::size/usage/cpuAccess (champs inexistants — le
		// vrai NkBufferDesc utilise sizeBytes/type(NkBufferType)/usage
		// (NkResourceUsage)/bindFlags) et cmd->BindUniformBuffer(handle, slot)
		// (inexistant sur NkICommandBuffer — le vrai BindUniformBuffer vit sur
		// NkIDevice et prend un NkDescSetHandle, pas un slot brut). Le CPU-side
		// (poids blendshapes) reste fonctionnel ; le trajet GPU réel est reporté
		// à la Phase R3 (réécriture sur le vrai NkBufferDesc/MapBuffer, branché
		// comme entrée uniforme d'un matériau custom NKRenderer).
		// =====================================================================
		bool NkBSDriver::Init(NkIDevice *device, nk_uint32 count) noexcept {
			mDevice = device;
			mCount = NkMin(count, kMaxBlendshapes);
			memset(mWeightsCPU, 0, sizeof(mWeightsCPU));

			if (!mDevice)
				return false;

			logger.Warnf("[NkBSDriver] Upload GPU stubé (Phase R1) — {} blendshapes calculés en CPU "
						 "uniquement, en attente de la réécriture NKRenderer (Phase R3)\n",
						 mCount);
			return true;
		}

		void NkBSDriver::Shutdown() noexcept {
			mGPUBuffer = {};
		}

		void NkBSDriver::SetWeights(const nk_float32 *weights, nk_uint32 count) noexcept {
			nk_uint32 n = NkMin(count, mCount);
			if (memcmp(mWeightsCPU, weights, n * sizeof(nk_float32)) == 0)
				return;
			memcpy(mWeightsCPU, weights, n * sizeof(nk_float32));
			// Remettre à zéro les poids restants
			if (n < kMaxBlendshapes)
				memset(mWeightsCPU + n, 0, (kMaxBlendshapes - n) * sizeof(nk_float32));
			mDirty = true;
		}

		void NkBSDriver::Flush(NkICommandBuffer *cmd) noexcept {
			(void)cmd;
			// Stub Phase R1 : pas de buffer GPU réel tant que R3 n'a pas branché
			// NkBSDriver sur le vrai NkBufferDesc/MapBuffer de NKRHI.
			mDirty = false;
		}

		void NkBSDriver::Bind(NkICommandBuffer *cmd) noexcept {
			(void)cmd;
			// Stub Phase R1 — no-op, cf. Flush().
		}

		void NkBSDriver::Unbind(NkICommandBuffer *cmd) noexcept {
			(void)cmd;
			// La plupart des backends ne nécessitent pas un unbind explicite
		}

	} // namespace pv3de
} // namespace nkentseu
