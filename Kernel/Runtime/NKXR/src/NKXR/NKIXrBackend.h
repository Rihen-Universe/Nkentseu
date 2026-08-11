//
// NKIXrBackend.h
// =============================================================================
// Description :
//   Le contrat qu'un backend XR doit remplir. Deux implémentations prévues :
//   le SIMULATEUR desktop (étage 0, aucun matériel) et OpenXR (étage 2,
//   Quest/Pico). L'API publique de NKXR ne doit RIEN laisser transparaître
//   du backend — c'est cette interface qui tient la frontière.
//
// Caractéristiques :
//   - Les espaces sont passés par TYPE (pas par objet) à cette frontière :
//     le backend est le seul à savoir ce qu'un espace « est » chez lui.
//   - Convention moteur : préfixe NKI pour les interfaces, destruction via
//     la fabrique (NkXrBackendFactory) qui remet le pointeur à nullptr —
//     même geste que NkDeviceFactory de NKRHI.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKIXRBACKEND_H__
#define __NKENTSEU_XR_NKIXRBACKEND_H__

#include "NKXR/NkXrTypes.h"
#include "NKXR/NkXrPose.h"
#include "NKXR/NkXrLayer.h"
#include "NKXR/NkXrInput.h"

namespace nkentseu {

	class NkWindow;

	namespace xr {

		// ── Choix du backend ─────────────────────────────────────────────────
		enum class NkXrBackendType : uint8 {
			NK_XR_BACKEND_SIMULATOR = 0,
			NK_XR_BACKEND_OPENXR = 1,    ///< Étage 2 — pas encore implémenté.
		};

		// ── Description de session ───────────────────────────────────────────
		struct NkXrSessionDesc {
			NkXrBackendType backend = NkXrBackendType::NK_XR_BACKEND_SIMULATOR;
			// Fenêtre hôte du simulateur (stéréo côte à côte + souris/clavier).
			// nullptr accepté : mode « sans tête » pour les tests numériques —
			// le simulateur tourne alors sur sa pose scriptée, sans entrées.
			NkWindow *window = nullptr;
			float32 ipdMeters = 0.063f;  ///< Écart interpupillaire.
			// Profondeur de la projection recommandée par GetSystemInfo — le
			// backend n'impose rien, il mémorise pour qui la lui demande.
			bool depthZeroToOne = false;
		};

		// ── Liaison graphique Vulkan (étape 2b) ──────────────────────────────
		// Le runtime d'un casque impose ses conditions au device Vulkan : des
		// extensions d'instance et de device, et LE VkPhysicalDevice à
		// utiliser. L'application interroge le backend AVANT de créer son
		// device NKRHI, puis lui remet les handles créés. void* partout : ces
		// headers restent agnostiques, seuls les .cpp parlent Vulkan.

		struct NkXrVulkanRequirements {
			// Listes d'extensions au format OpenXR : noms séparés par des
			// espaces (c'est ce que rendent les fonctions xrGetVulkan*).
			char instanceExtensions[1024] = {};
			char deviceExtensions[1024] = {};
		};

		struct NkXrVulkanBinding {
			void *instance = nullptr;        ///< VkInstance
			void *physicalDevice = nullptr;  ///< VkPhysicalDevice
			void *device = nullptr;          ///< VkDevice
			uint32 queueFamilyIndex = 0;
			uint32 queueIndex = 0;
		};

		// ── Le contrat backend ───────────────────────────────────────────────
		class NKIXrBackend {
			public:
				virtual ~NKIXrBackend() = default;

				virtual bool Initialize(const NkXrSessionDesc &desc) = 0;
				virtual void Shutdown() = 0;

				// Liaison graphique — défauts NEUTRES : le simulateur rend des
				// exigences vides et accepte la liaison sans rien en faire, si
				// bien qu'une app écrite pour le casque tourne telle quelle
				// sur lui. false = le backend EXIGE la liaison et ne l'a pas.
				virtual bool GetVulkanRequirements(NkXrVulkanRequirements &outRequirements) {
					outRequirements = NkXrVulkanRequirements{};
					return true;
				}
				virtual void *GetVulkanPhysicalDevice(void *vkInstance) {
					(void)vkInstance;
					return nullptr;
				}
				virtual bool BindVulkan(const NkXrVulkanBinding &binding) {
					(void)binding;
					return true;
				}

				// Swapchains du CASQUE (2b.2) — le backend possède les images
				// du compositeur ; l'app rend chez elle puis lui REMET ses
				// images d'œil (handles natifs opaques — VkImage sous Vulkan)
				// et il compose : copie vers l'image acquise + couche de
				// projection au prochain EndFrame. Le simulateur accepte tout
				// sans rien faire : son « casque » est la fenêtre, déjà servie.
				virtual bool CreateHmdSwapchains(uint32 width, uint32 height) {
					(void)width;
					(void)height;
					return true;
				}
				virtual bool SubmitEyes(const NkXrView views[NK_XR_EYE_COUNT], uint64 nativeImageLeft,
										uint64 nativeImageRight, uint32 width, uint32 height) {
					(void)views;
					(void)nativeImageLeft;
					(void)nativeImageRight;
					(void)width;
					(void)height;
					return true;
				}

				virtual NkXrSystemInfo GetSystemInfo() const = 0;
				virtual NkXrSessionState GetState() const = 0;
				virtual bool PollEvent(NkXrEvent &outEvent) = 0;

				virtual bool BeginSession() = 0;
				virtual bool EndSession() = 0;
				virtual void RequestExit() = 0;

				virtual bool WaitFrame(NkXrFrameState &outState) = 0;
				virtual bool BeginFrame() = 0;
				virtual bool EndFrame(const NkXrFrameEndInfo &info) = 0;

				// Poses des deux yeux dans l'espace demandé, PRÉDITES pour
				// displayTime (celui du WaitFrame de la frame en cours).
				virtual bool LocateViews(NkXrSpaceType space, NkXrTime displayTime, NkXrView outViews[NK_XR_EYE_COUNT]) = 0;

				// Pose de l'origine de « space » exprimée dans « base ».
				virtual bool LocateSpace(NkXrSpaceType space, NkXrSpaceType base, NkXrTime time, NkXrPose &outPose) = 0;

				virtual void AttachActions(const NkXrActionDesc *actions, uint32 count) = 0;
				virtual bool SyncActions(NkXrTime now) = 0;
				virtual bool GetActionStateBool(NkXrActionHandle handle, NkXrActionStateBool &outState) = 0;
				virtual bool GetActionStateFloat(NkXrActionHandle handle, NkXrActionStateFloat &outState) = 0;
				virtual bool GetActionStateVec2(NkXrActionHandle handle, NkXrActionStateVec2 &outState) = 0;
				virtual bool LocateActionPose(NkXrActionHandle handle, NkXrSpaceType space, NkXrTime time, NkXrPose &outPose) = 0;
		};

		// ── Fabrique (pattern NkDeviceFactory de NKRHI) ──────────────────────
		class NkXrBackendFactory {
			public:
				static NKIXrBackend *Create(const NkXrSessionDesc &desc);
				// Référence de pointeur : Shutdown + libération + nullptr, pour
				// qu'un backend détruit ne puisse pas survivre par une copie
				// oubliée du pointeur.
				static void Destroy(NKIXrBackend *&backend);
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKIXRBACKEND_H__
