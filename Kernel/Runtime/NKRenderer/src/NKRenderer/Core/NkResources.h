#pragma once
// =============================================================================
// NkResources.h  — NKRenderer v5.0  (Core/)
//
// Helpers RHI centralises pour eviter la duplication entre sous-systemes
// (Render2D, Render3D, Materials, Shadow, PostProcess...).
//
// Couvre :
//   1. Default textures (1x1 white/black/normal/magenta-fallback)
//      -> rappel : ces textures sont aussi exposees par NkTextureLibrary
//         (GetWhite1x1 etc.). NkResources stocke les handles RHI bruts ;
//         NkTextureLibrary les wrappe en NkTexHandle.
//
//   2. Default samplers (Linear/Nearest x Repeat/Clamp/Border, Anisotropic16,
//      Shadow comparaison-sampler)
//
//   3. Standard descriptor-set layouts par convention UE5/UPBGE :
//        - Frame (set 0) : camera + lights + IBL + shadow atlas
//        - Object (set 1) : modele + bones + per-instance
//        - Material (set 2) : PBR params + 5 textures (albedo, normal, ORM, emissive, AO)
//        - PostProcess (set 3) : sampler 2D unique + push constants
//
//   4. Buffer factories : CreateUBO / CreateSSBO / CreateVertexDynamic / etc.
//
// La ressource centrale est cree une seule fois au demarrage du renderer ;
// tous les sous-systemes la consomment en lecture (jamais en mutation).
// =============================================================================
#include "NkRendererTypes.h"
#include "NkRendererResult.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		// =====================================================================
		// Convention sets (numerotation UE-style)
		// =====================================================================
		enum NkDescSetIndex : uint32 {
			NK_SET_FRAME = 0,		// camera, lights, IBL, shadow
			NK_SET_OBJECT = 1,		// model, bones, instance data
			NK_SET_MATERIAL = 2,	// PBR params + textures
			NK_SET_POSTPROCESS = 3, // input image + sampler
		};

		// Bindings dans le set Frame (set=0)
		enum NkFrameBinding : uint32 {
			NK_BIND_CAMERA_UBO = 0,
			NK_BIND_LIGHTS_UBO = 1,
			NK_BIND_LIGHTS_SSBO = 2, // forward+ light-list
			NK_BIND_CLUSTERS_SSBO = 3,
			NK_BIND_SHADOW_ATLAS = 4,
			NK_BIND_IBL_IRRADIANCE = 5,
			NK_BIND_IBL_SPECULAR = 6,
			NK_BIND_BRDF_LUT = 7,
		};

		// Bindings dans le set Object (set=1)
		enum NkObjectBinding : uint32 {
			NK_BIND_OBJECT_UBO = 0,
			NK_BIND_BONES_SSBO = 1,
			NK_BIND_INSTANCE_SSBO = 2,
		};

		// Bindings dans le set Material (set=2)
		enum NkMaterialBinding : uint32 {
			NK_BIND_PBR_PARAMS = 0, // UBO
			NK_BIND_TEX_ALBEDO = 1,
			NK_BIND_TEX_NORMAL = 2,
			NK_BIND_TEX_ORM = 3, // O=AO, R=Roughness, M=Metallic
			NK_BIND_TEX_EMISSIVE = 4,
			NK_BIND_TEX_AO = 5, // AO separe (override de l'ORM si fourni)
		};

		// Bindings dans le set PostProcess (set=3)
		enum NkPostProcessBinding : uint32 {
			NK_BIND_PP_INPUT = 0, // image d'entree du passe plein ecran
		};

		// =====================================================================
		// LA TABLE DES BINDINGS STANDARD — set + binding, DANS UNE DECLARATION
		// =====================================================================
		// POURQUOI CETTE TABLE EXISTE (2026-08-22)
		//
		// Jusqu'ici, l'appartenance d'un binding a un set ne vivait QUE dans un
		// commentaire (« Bindings dans le set Frame (set=0) ») et dans un nom de
		// membre (mFrameLayout). AUCUNE DECLARATION NE LA PORTAIT.
		//
		// Consequence mesuree : un meme numero de binding existe dans plusieurs
		// sets a la fois — sans son set, un numero ne veut rien dire. Aucun outil
		// ne pouvait donc confronter ce que le C++ LIE a ce que les shaders
		// ECHANTILLONNENT (layout(set=S, binding=N)) : il aurait fallu DEVINER
		// l'appariement enum -> set. Un detecteur qui devine fabrique des faux
		// positifs, et un detecteur qui fabrique des faux positifs se desactive.
		//
		// Un commentaire ne peut pas se tromper : il peut seulement etre faux sans
		// que rien ne le dise. Cette table, elle, est LUE par le code —
		// CreateStandardLayouts() la parcourt. Si elle ment, les layouts mentent
		// avec elle, et ca se voit.
		//
		// AJOUTER UN BINDING = AJOUTER UNE LIGNE ICI. Un NK_BIND_* declare dans un
		// enum ci-dessus mais absent de cette table est signale par
		// ./verif_capacites.sh (detecteur D3). Ce controle est une RELATION
		// (« tout enum est dans la table »), jamais un compte fige : la table est
		// censee grandir, et un nombre en dur serait une dette a echeance.
		// ATTENTION AU NOM QUI EXISTE DEUX FOIS. Deux types NkShaderStage sont
		// visibles depuis cet en-tete :
		//   nkentseu::NkShaderStage           (= NkSLStage, celui du RHI, qui
		//                                       porte NK_ALL_GRAPHICS)
		//   nkentseu::renderer::NkShaderStage (NkShaderBackend.h, un enum a lui)
		// Non qualifie, le SECOND gagne dans tout .cpp qui inclut les deux — et
		// l'erreur ne sort pas ici, elle sort chez l'includeur. Mesure du 22/08 :
		// NkResources.cpp compilait, NkRender3D.cpp non. Tout est qualifie.
		struct NkStandardBinding {
			NkDescSetIndex set;
			uint32 binding;
			NkDescriptorType type;
			::nkentseu::NkShaderStage stages; // QUALIFIE : voir la note ci-dessous
			const char *name; // le nom de la constante, pour les outils et les journaux
		};

		static constexpr NkStandardBinding kStandardBindings[] = {
			// --- set 0 : Frame ---
			{NK_SET_FRAME, NK_BIND_CAMERA_UBO, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, "NK_BIND_CAMERA_UBO"},
			{NK_SET_FRAME, NK_BIND_LIGHTS_UBO, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_LIGHTS_UBO"},
			{NK_SET_FRAME, NK_BIND_LIGHTS_SSBO, NkDescriptorType::NK_STORAGE_BUFFER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_LIGHTS_SSBO"},
			{NK_SET_FRAME, NK_BIND_CLUSTERS_SSBO, NkDescriptorType::NK_STORAGE_BUFFER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_CLUSTERS_SSBO"},
			{NK_SET_FRAME, NK_BIND_SHADOW_ATLAS, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_SHADOW_ATLAS"},
			{NK_SET_FRAME, NK_BIND_IBL_IRRADIANCE, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_IBL_IRRADIANCE"},
			{NK_SET_FRAME, NK_BIND_IBL_SPECULAR, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_IBL_SPECULAR"},
			{NK_SET_FRAME, NK_BIND_BRDF_LUT, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_BRDF_LUT"},
			// --- set 1 : Object ---
			{NK_SET_OBJECT, NK_BIND_OBJECT_UBO, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, "NK_BIND_OBJECT_UBO"},
			{NK_SET_OBJECT, NK_BIND_BONES_SSBO, NkDescriptorType::NK_STORAGE_BUFFER, ::nkentseu::NkShaderStage::NK_VERTEX, "NK_BIND_BONES_SSBO"},
			{NK_SET_OBJECT, NK_BIND_INSTANCE_SSBO, NkDescriptorType::NK_STORAGE_BUFFER, ::nkentseu::NkShaderStage::NK_VERTEX, "NK_BIND_INSTANCE_SSBO"},
			// --- set 2 : Material ---
			{NK_SET_MATERIAL, NK_BIND_PBR_PARAMS, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_PBR_PARAMS"},
			{NK_SET_MATERIAL, NK_BIND_TEX_ALBEDO, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_TEX_ALBEDO"},
			{NK_SET_MATERIAL, NK_BIND_TEX_NORMAL, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_TEX_NORMAL"},
			{NK_SET_MATERIAL, NK_BIND_TEX_ORM, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_TEX_ORM"},
			{NK_SET_MATERIAL, NK_BIND_TEX_EMISSIVE, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_TEX_EMISSIVE"},
			{NK_SET_MATERIAL, NK_BIND_TEX_AO, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_TEX_AO"},
			// --- set 3 : PostProcess ---
			{NK_SET_POSTPROCESS, NK_BIND_PP_INPUT, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_FRAGMENT, "NK_BIND_PP_INPUT"},
		};

		static constexpr uint32 kStandardBindingCount =
			(uint32)(sizeof(kStandardBindings) / sizeof(kStandardBindings[0]));

		// =====================================================================
		// NkResources
		// =====================================================================
		class NkResources {
			public:
				NkResources() = default;
				~NkResources();

				NkResources(const NkResources &) = delete;
				NkResources &operator=(const NkResources &) = delete;

				NkRResult Init(NkIDevice *device);
				void Shutdown();

				bool IsReady() const noexcept {
					return mReady;
				}

				// ── Default textures (1x1) ────────────────────────────────────
				// Utiles comme fallback quand un slot de material est vide.
				NkTextureHandle GetWhiteTex() const noexcept {
					return mTexWhite;
				}

				NkTextureHandle GetBlackTex() const noexcept {
					return mTexBlack;
				}

				NkTextureHandle GetNormalTex() const noexcept {
					return mTexNormal;
				} // (0.5, 0.5, 1, 1)

				NkTextureHandle GetMagentaTex() const noexcept {
					return mTexMagenta;
				} // missing-tex marker

				NkTextureHandle GetGrayTex() const noexcept {
					return mTexGray;
				} // (0.5, 0.5, 0.5, 1)

				// ── Default samplers ──────────────────────────────────────────
				NkSamplerHandle GetSamplerLinearRepeat() const noexcept {
					return mSamLinearRepeat;
				}

				NkSamplerHandle GetSamplerLinearClamp() const noexcept {
					return mSamLinearClamp;
				}

				NkSamplerHandle GetSamplerLinearBorder() const noexcept {
					return mSamLinearBorder;
				}

				NkSamplerHandle GetSamplerNearestRepeat() const noexcept {
					return mSamNearestRepeat;
				}

				NkSamplerHandle GetSamplerNearestClamp() const noexcept {
					return mSamNearestClamp;
				}

				NkSamplerHandle GetSamplerAnisotropic16() const noexcept {
					return mSamAniso16;
				}

				NkSamplerHandle GetSamplerShadow() const noexcept {
					return mSamShadow;
				} // PCF compare-sampler

				NkSamplerHandle GetSamplerCubemap() const noexcept {
					return mSamCubemap;
				} // tri-linear, clamp

				// ── Standard descriptor set layouts ───────────────────────────
				NkDescSetHandle GetFrameLayout() const noexcept {
					return mFrameLayout;
				}

				NkDescSetHandle GetObjectLayout() const noexcept {
					return mObjectLayout;
				}

				NkDescSetHandle GetMaterialLayout() const noexcept {
					return mMaterialLayout;
				}

				NkDescSetHandle GetPostProcessLayout() const noexcept {
					return mPostProcessLayout;
				}

				// ── Buffer factories ──────────────────────────────────────────
				// Tous renvoient NkBufferHandle::Null si echec.
				NkBufferHandle CreateUBO(uint64 sizeBytes, const char *debugName = nullptr);
				NkBufferHandle CreateSSBO(uint64 sizeBytes, const char *debugName = nullptr);
				NkBufferHandle CreateVertexDynamic(uint64 sizeBytes, const char *debugName = nullptr);
				NkBufferHandle CreateIndexBuffer(const uint32 *data, uint32 count, const char *debugName = nullptr);
				NkBufferHandle CreateStagingBuffer(uint64 sizeBytes, const char *debugName = nullptr);

			private:
				NkIDevice *mDevice = nullptr;
				bool mReady = false;

				NkTextureHandle mTexWhite, mTexBlack, mTexNormal, mTexMagenta, mTexGray;

				NkSamplerHandle mSamLinearRepeat, mSamLinearClamp, mSamLinearBorder;
				NkSamplerHandle mSamNearestRepeat, mSamNearestClamp;
				NkSamplerHandle mSamAniso16, mSamShadow, mSamCubemap;

				NkDescSetHandle mFrameLayout, mObjectLayout, mMaterialLayout, mPostProcessLayout;

				bool CreateDefaultTextures();
				bool CreateDefaultSamplers();
				bool CreateStandardLayouts();
		};

	} // namespace renderer
} // namespace nkentseu
