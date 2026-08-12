//
// NkXrSession.h
// =============================================================================
// Description :
//   La session XR : le point d'entrée unique de l'application. Cycle de vie,
//   boucle de frame (WaitFrame → BeginFrame → LocateViews → rendu →
//   EndFrame), espaces, swapchains, actions — tout passe par elle.
//
// Caractéristiques :
//   - Enveloppe mince et VIGILANTE autour du backend : c'est ici que les
//     transitions d'état illégales et les frames mal imbriquées sont
//     refusées ET DITES (journal), pour que la démo se comporte déjà comme
//     l'exigera un runtime de casque — qui, lui, sanctionnera en silence.
//   - Possède les swapchains (création/destruction par NKMemory) ; le
//     backend, lui, ne possède que le tracking et le matériel.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRSESSION_H__
#define __NKENTSEU_XR_NKXRSESSION_H__

#include "NKXR/NKIXrBackend.h"
#include "NKXR/NkXrSpace.h"
#include "NKXR/NkXrSwapchain.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		class NkXrSession {
			public:
				static NkXrSession *Create(const NkXrSessionDesc &desc);
				static void Destroy(NkXrSession *&session);

				// ── Liaison graphique (étape 2b, backend casque) ─────────────
				// À appeler dans cet ordre : requirements AVANT la création du
				// device NKRHI, physical device PENDANT (via le crochet
				// NkVulkanDesc.pickPhysicalDevice), Bind APRÈS.
				bool GetVulkanRequirements(NkXrVulkanRequirements &outRequirements);
				void *GetVulkanPhysicalDevice(void *vkInstance);
				bool BindVulkan(const NkXrVulkanBinding &binding);
				bool CreateHmdSwapchains(uint32 width, uint32 height);
				bool SubmitEyes(const NkXrView views[NK_XR_EYE_COUNT], uint64 nativeImageLeft,
								uint64 nativeImageRight, uint32 width, uint32 height, uint64 depthLeft = 0,
								uint64 depthRight = 0, float32 nearZ = 0.f, float32 farZ = 0.f);

				// ── Système ──────────────────────────────────────────────────
				NkXrSystemInfo GetSystemInfo() const;
				NkXrSessionState GetState() const;
				bool PollEvent(NkXrEvent &outEvent);

				// ── Cycle de vie ─────────────────────────────────────────────
				bool Begin();
				bool End();
				void RequestExit();

				// ── Boucle de frame ──────────────────────────────────────────
				bool WaitFrame(NkXrFrameState &outState);
				bool BeginFrame();
				bool EndFrame(const NkXrFrameEndInfo &info);

				// ── Vues et espaces ──────────────────────────────────────────
				bool LocateViews(const NkXrSpace &space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]);
				bool LocateSpace(const NkXrSpace &space, const NkXrSpace &base, NkXrTime time, NkXrPose &outPose);

				// ── Swapchains ───────────────────────────────────────────────
				NkXrSwapchain *CreateSwapchain(const NkXrSwapchainDesc &desc);
				void DestroySwapchain(NkXrSwapchain *&swapchain);

				// ── Actions ──────────────────────────────────────────────────
				// Attacher fige le jeu d'actions (comme xrAttachSessionActionSets) :
				// le backend construit ses liaisons une fois, pas à chaque sync.
				bool AttachActionSet(const NkXrActionSet &set);
				bool SyncActions();
				bool GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState);
				bool GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState);
				bool GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState);
				bool LocateActionPose(NkXrActionHandle handle, const NkXrSpace &space, NkXrTime time, NkXrPose &outPose);
				bool ApplyHaptic(NkXrActionHandle handle, float32 amplitude, float32 durationSeconds, float32 frequencyHz = 0.f);
				// Vraies mains (sans manettes) — false si le tracking des mains
				// n'existe pas ou ne voit pas la main : retomber sur les manettes.
				bool LocateHand(NkXrHandSide side, const NkXrSpace &space, NkXrTime time, NkXrHand &outHand);

				// ── Mesure et réglage de la restitution ──────────────────────
				bool GetPerfMetrics(NkXrPerfMetrics &outMetrics);
				uint32 GetDisplayRefreshRates(float32 *outRates, uint32 capacity);
				float32 GetDisplayRefreshRate();
				bool RequestDisplayRefreshRate(float32 hz);
				bool GetVisibilityMask(NkXrEye eye, NkXrVisibilityMask &outMask);

				// Temps XR « maintenant » (l'horloge que datent poses et frames).
				static NkXrTime Now();

				// Constructeurs publics pour l'allocateur (New<T> fait le
				// placement new hors de la classe) — mais le chemin documenté
				// reste Create/Destroy, qui seuls gèrent backend et swapchains.
				NkXrSession() = default;
				~NkXrSession() = default;

			private:
				NKIXrBackend *mBackend = nullptr;
				NkVector<NkXrSwapchain *> mSwapchains;
				bool mFrameOpen = false;
				bool mActionsAttached = false;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRSESSION_H__
