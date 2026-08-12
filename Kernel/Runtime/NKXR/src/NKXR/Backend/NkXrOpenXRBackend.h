//
// NkXrOpenXRBackend.h
// =============================================================================
// Description :
//   Le backend n°2 de NKXR : les VRAIS casques, via le protocole OpenXR
//   (Quest, Pico, runtimes PC). Étape 2a livrée ici : chargement DYNAMIQUE,
//   négociation, instance, système, tailles recommandées — le « bonjour » au
//   casque. Étape 2b (liaison Vulkan : session, swapchains, boucle de frame)
//   viendra ensuite ; chaque appel non couvert le DIT au journal au lieu de
//   faire semblant.
//
// Caractéristiques :
//   - AUCUN link statique : le loader (openxr_loader.dll / libopenxr_loader.so
//     de Meta sur Quest) est chargé à l'exécution — le pattern XInput du
//     moteur, qui évite qu'une DLL absente empêche l'exe de DÉMARRER.
//   - À défaut de loader, sur Windows, DÉCOUVERTE DIRECTE du runtime actif
//     (registre Khronos → manifeste json → bibliothèque → négociation) :
//     code Nkentseu, ce qui permet de tester sans expédier de loader.
//   - Ce header n'expose AUCUN type OpenXR : l'état vit dans un struct opaque
//     alloué par le .cpp — les consommateurs de NKXR ne voient jamais
//     l'API C d'OpenXR, c'est la frontière promise par la mission.
//
// Crochets d'environnement :
//   NK_XR_OPENXR_LOADER=<chemin>   bibliothèque loader à charger en priorité
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXROPENXRBACKEND_H__
#define __NKENTSEU_XR_NKXROPENXRBACKEND_H__

#include "NKXR/NKIXrBackend.h"

namespace nkentseu {
	namespace xr {

		class NkXrOpenXRBackend final : public NKIXrBackend {
			public:
				NkXrOpenXRBackend() = default;
				~NkXrOpenXRBackend() override;

				bool Initialize(const NkXrSessionDesc &desc) override;
				void Shutdown() override;

				// Liaison Vulkan (étape 2b) : le runtime dicte, NKRHI exécute.
				bool GetVulkanRequirements(NkXrVulkanRequirements &outRequirements) override;
				void *GetVulkanPhysicalDevice(void *vkInstance) override;
				bool BindVulkan(const NkXrVulkanBinding &binding) override;
				// 2b.2 : les images du compositeur + la copie de nos yeux vers
				// elles + la couche de projection au EndFrame.
				bool CreateHmdSwapchains(uint32 width, uint32 height) override;
				bool SubmitEyes(const NkXrView views[NK_XR_EYE_COUNT], uint64 nativeImageLeft,
								uint64 nativeImageRight, uint32 width, uint32 height) override;

				NkXrSystemInfo GetSystemInfo() const override;
				NkXrSessionState GetState() const override;
				bool PollEvent(NkXrEvent &outEvent) override;

				bool BeginSession() override;
				bool EndSession() override;
				void RequestExit() override;

				bool WaitFrame(NkXrFrameState &outState) override;
				bool BeginFrame() override;
				bool EndFrame(const NkXrFrameEndInfo &info) override;

				bool LocateViews(NkXrSpaceType space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) override;
				bool LocateSpace(NkXrSpaceType space, NkXrSpaceType base, NkXrTime time, NkXrPose &outPose) override;

				void AttachActions(const NkXrActionDesc *actions, uint32 count) override;
				bool SyncActions(NkXrTime now) override;
				bool ApplyHaptic(NkXrActionHandle handle, float32 amplitude, float32 durationSeconds,
								 float32 frequencyHz) override;
				bool LocateHand(NkXrHandSide side, NkXrSpaceType space, NkXrTime time, NkXrHand &outHand) override;
				bool GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) override;
				bool GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) override;
				bool GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) override;
				bool LocateActionPose(NkXrActionHandle handle, NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) override;

			private:
				// Tout l'état OpenXR (handles, pointeurs de fonctions) vit dans
				// ce struct opaque défini par le .cpp — voir « frontière ».
				struct OxrState;
				OxrState *mOxr = nullptr;

				// Crée jeu d'actions + liaisons (profils Touch et simple) puis
				// attache — appelé dès que session ET descriptions existent.
				void SetupActions();

				NkXrSessionDesc mDesc{};
				NkXrSessionState mState = NkXrSessionState::NK_XR_STATE_IDLE;
				NkXrSystemInfo mSystemInfo{};
				char mSystemName[256] = "OpenXR (non initialisé)";
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXROPENXRBACKEND_H__
