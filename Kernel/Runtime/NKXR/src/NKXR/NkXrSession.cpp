//
// NkXrSession.cpp
// =============================================================================
// Description :
//   Implémentation de NkXrSession : garde-fous du cycle de vie et de la
//   boucle de frame, possession des swapchains, délégation au backend.
//
// Caractéristiques :
//   - Chaque refus est JOURNALISÉ : une transition illégale silencieuse est
//     la première cause de « ça marche sur le simulateur, pas sur le casque ».
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/NkXrSession.h"
#include "NKXR/Backend/NkXrSimulatorBackend.h"
#include "NKXR/Backend/NkXrOpenXRBackend.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {
	namespace xr {

		// ── Fabrique de backend ──────────────────────────────────────────────

		NKIXrBackend *NkXrBackendFactory::Create(const NkXrSessionDesc &desc) {
			NKIXrBackend *backend = nullptr;
			switch (desc.backend) {
				case NkXrBackendType::NK_XR_BACKEND_SIMULATOR: {
					backend = memory::NkGetDefaultAllocator().New<NkXrSimulatorBackend>();
					break;
				}
				case NkXrBackendType::NK_XR_BACKEND_OPENXR: {
					// Étape 2a : négociation + instance + système. La session
					// de casque (liaison Vulkan) arrive en 2b — Initialize
					// échoue proprement si aucun runtime/casque n'est là.
					backend = memory::NkGetDefaultAllocator().New<NkXrOpenXRBackend>();
					break;
				}
			}
			if (backend != nullptr && !backend->Initialize(desc)) {
				memory::NkGetDefaultAllocator().Delete(backend);
				return nullptr;
			}
			return backend;
		}

		void NkXrBackendFactory::Destroy(NKIXrBackend *&backend) {
			if (backend == nullptr) {
				return;
			}
			backend->Shutdown();
			memory::NkGetDefaultAllocator().Delete(backend);
			backend = nullptr;
		}

		// ── Cycle de vie de la session ───────────────────────────────────────

		NkXrSession *NkXrSession::Create(const NkXrSessionDesc &desc) {
			NKIXrBackend *backend = NkXrBackendFactory::Create(desc);
			if (backend == nullptr) {
				return nullptr;
			}
			NkXrSession *session = memory::NkGetDefaultAllocator().New<NkXrSession>();
			if (session == nullptr) {
				NkXrBackendFactory::Destroy(backend);
				return nullptr;
			}
			session->mBackend = backend;
			logger.Infof("[NKXR] Session créée — système : %s\n", session->GetSystemInfo().systemName);
			return session;
		}

		void NkXrSession::Destroy(NkXrSession *&session) {
			if (session == nullptr) {
				return;
			}
			// Les swapchains survivantes sont libérées ici : l'app qui oublie
			// les siennes ne fuit pas, mais le journal le dit — c'est un bug
			// d'app, pas un service.
			if (session->mSwapchains.Size() != 0u) {
				logger.Warnf("[NKXR] %u swapchain(s) non détruite(s) à la destruction de la session.\n",
							 uint32(session->mSwapchains.Size()));
				for (nk_size i = 0; i < session->mSwapchains.Size(); ++i) {
					memory::NkGetDefaultAllocator().Delete(session->mSwapchains[i]);
				}
			}
			NkXrBackendFactory::Destroy(session->mBackend);
			memory::NkGetDefaultAllocator().Delete(session);
			session = nullptr;
		}

		bool NkXrSession::GetVulkanRequirements(NkXrVulkanRequirements &outRequirements) {
			return mBackend->GetVulkanRequirements(outRequirements);
		}

		void *NkXrSession::GetVulkanPhysicalDevice(void *vkInstance) {
			return mBackend->GetVulkanPhysicalDevice(vkInstance);
		}

		bool NkXrSession::BindVulkan(const NkXrVulkanBinding &binding) {
			return mBackend->BindVulkan(binding);
		}

		NkXrSystemInfo NkXrSession::GetSystemInfo() const {
			return mBackend->GetSystemInfo();
		}

		NkXrSessionState NkXrSession::GetState() const {
			return mBackend->GetState();
		}

		bool NkXrSession::PollEvent(NkXrEvent &outEvent) {
			return mBackend->PollEvent(outEvent);
		}

		bool NkXrSession::Begin() {
			if (mBackend->GetState() != NkXrSessionState::NK_XR_STATE_READY) {
				logger.Errorf("[NKXR] Begin() refusé : la session n'est pas READY (état %d).\n", int(mBackend->GetState()));
				return false;
			}
			return mBackend->BeginSession();
		}

		bool NkXrSession::End() {
			const NkXrSessionState state = mBackend->GetState();
			if (state != NkXrSessionState::NK_XR_STATE_STOPPING) {
				logger.Errorf("[NKXR] End() refusé : attendu en STOPPING seulement (état %d).\n", int(state));
				return false;
			}
			return mBackend->EndSession();
		}

		void NkXrSession::RequestExit() {
			mBackend->RequestExit();
		}

		// ── Boucle de frame ──────────────────────────────────────────────────

		bool NkXrSession::WaitFrame(NkXrFrameState &outState) {
			if (mFrameOpen) {
				logger.Errorf("[NKXR] WaitFrame() refusé : la frame précédente n'a pas été soumise (EndFrame manquant).\n");
				return false;
			}
			return mBackend->WaitFrame(outState);
		}

		bool NkXrSession::BeginFrame() {
			if (mFrameOpen) {
				logger.Errorf("[NKXR] BeginFrame() refusé : frame déjà ouverte.\n");
				return false;
			}
			if (!mBackend->BeginFrame()) {
				return false;
			}
			mFrameOpen = true;
			return true;
		}

		bool NkXrSession::EndFrame(const NkXrFrameEndInfo &info) {
			if (!mFrameOpen) {
				logger.Errorf("[NKXR] EndFrame() refusé : aucune frame ouverte.\n");
				return false;
			}
			// Une couche qui référence une image encore acquise est le bug de
			// synchronisation classique : l'attraper ICI, où il est nommable,
			// plutôt que comme un scintillement inexpliqué à l'écran.
			if (info.projection != nullptr) {
				for (uint32 eye = 0u; eye < NK_XR_EYE_COUNT; ++eye) {
					const NkXrSwapchain *sc = info.projection->views[eye].swapchain;
					if (sc != nullptr && sc->IsAcquired()) {
						logger.Errorf("[NKXR] EndFrame() : la swapchain de l'œil %u est encore acquise (ReleaseImage manquant).\n", eye);
						mFrameOpen = false;
						return false;
					}
				}
			}
			mFrameOpen = false;
			return mBackend->EndFrame(info);
		}

		// ── Vues et espaces ──────────────────────────────────────────────────

		bool NkXrSession::LocateViews(const NkXrSpace &space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) {
			return mBackend->LocateViews(space.GetType(), displayTime, outViews);
		}

		bool NkXrSession::LocateSpace(const NkXrSpace &space, const NkXrSpace &base, NkXrTime time, NkXrPose &outPose) {
			return mBackend->LocateSpace(space.GetType(), base.GetType(), time, outPose);
		}

		// ── Swapchains ───────────────────────────────────────────────────────

		NkXrSwapchain *NkXrSession::CreateSwapchain(const NkXrSwapchainDesc &desc) {
			NkXrSwapchain *swapchain = memory::NkGetDefaultAllocator().New<NkXrSwapchain>();
			if (swapchain == nullptr) {
				return nullptr;
			}
			if (!swapchain->Init(desc)) {
				logger.Errorf("[NKXR] CreateSwapchain refusé : desc invalide (%ux%u, %u images).\n",
							  desc.width, desc.height, desc.imageCount);
				memory::NkGetDefaultAllocator().Delete(swapchain);
				return nullptr;
			}
			mSwapchains.PushBack(swapchain);
			return swapchain;
		}

		void NkXrSession::DestroySwapchain(NkXrSwapchain *&swapchain) {
			if (swapchain == nullptr) {
				return;
			}
			for (nk_size i = 0; i < mSwapchains.Size(); ++i) {
				if (mSwapchains[i] == swapchain) {
					mSwapchains[i] = mSwapchains[mSwapchains.Size() - 1u];
					mSwapchains.PopBack();
					break;
				}
			}
			memory::NkGetDefaultAllocator().Delete(swapchain);
			swapchain = nullptr;
		}

		// ── Actions ──────────────────────────────────────────────────────────

		bool NkXrSession::AttachActionSet(const NkXrActionSet &set) {
			if (mActionsAttached) {
				// Même règle qu'OpenXR : attacher fige. Ré-attacher en cours de
				// route invaliderait les handles déjà distribués à l'app.
				logger.Errorf("[NKXR] AttachActionSet refusé : un jeu d'actions est déjà attaché.\n");
				return false;
			}
			mBackend->AttachActions(set.Data(), set.Count());
			mActionsAttached = true;
			return true;
		}

		bool NkXrSession::SyncActions() {
			if (!mActionsAttached) {
				return false;
			}
			// Les entrées ne sont routées qu'en FOCUSED — c'est le contrat qui
			// évite qu'une app reçoive les clics pendant qu'un menu système
			// (ou une autre app du casque) a la main.
			if (mBackend->GetState() != NkXrSessionState::NK_XR_STATE_FOCUSED) {
				return false;
			}
			return mBackend->SyncActions(Now());
		}

		bool NkXrSession::GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) {
			return mBackend->GetActionStateBool(handle, outState);
		}

		bool NkXrSession::GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) {
			return mBackend->GetActionStateFloat(handle, outState);
		}

		bool NkXrSession::GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) {
			return mBackend->GetActionStateVec2(handle, outState);
		}

		bool NkXrSession::LocateActionPose(NkXrActionHandle handle, const NkXrSpace &space, NkXrTime time, NkXrPose &outPose) {
			return mBackend->LocateActionPose(handle, space.GetType(), time, outPose);
		}

		NkXrTime NkXrSession::Now() {
			// float64 → int64 : ~104 jours de nanosecondes tiennent dans la
			// mantisse d'un double, largement au-delà d'une session.
			return NkXrTime(NkChrono::Now().nanoseconds);
		}

	} // namespace xr
} // namespace nkentseu
