#pragma once
// =============================================================================
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PixolSculpt/NkSculptPipelines.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkSculptPipelines.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Registre des pipelines compute du sculpt pixol. S'appuie sur NkComputeContext
// (GetOrCompileGLSL) : les kernels GLSL embarques sont compiles + caches par le
// contexte.
//
// ⚠️ OU VIT LE KERNEL QUI TOURNE VRAIMENT. Pas dans shaders/ : le chemin
//    actif compile la chaine GLSL ECRITE EN DUR dans NkSculptPipelines.cpp. Les
//    14 fichiers de shaders/ (6 dialectes) ne sont lus par PERSONNE aujourd'hui
//    -- ils sont une reference, pas une source. Modifier un .glsl du dossier
//    sans toucher le .cpp ne change donc RIEN a l'execution : c'est le genre de
//    correctif qui ne deplace aucune mesure, et qui passe pour un placebo.
//
// ⚠️ NkSL n'est pas utilise pour le chemin actif -- parce que GetOrCompileGLSL
//    prend du GLSL, et NON parce que NkSL serait non fonctionnel : il l'est sur
//    5 dorsaux sur 6 depuis 06/2026. Ce qui manque est son branchement dans le
//    LOADER du renderer. (Precision du 19/09/2026 : la formulation d'avant se
//    lisait comme un verdict sur le langage.)
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {

	class NkComputeContext; // NKRHI/Core/NkComputeContext.h

	namespace renderer {

		class NkSculptPipelines {
			public:
				NkSculptPipelines() noexcept = default;
				~NkSculptPipelines() noexcept = default;

				bool Init(NkComputeContext *ctx) noexcept;
				void Shutdown() noexcept;

				[[nodiscard]] bool IsValid() const noexcept {
					return mCtx != nullptr;
				}

				// Un seul kernel de brosse (mode via push-constant).
				[[nodiscard]] NkPipelineHandle Brush() noexcept;
				// Composite pixol -> G-buffer.
				[[nodiscard]] NkPipelineHandle Resolve() noexcept;

			private:
				NkComputeContext *mCtx = nullptr;
		};

	} // namespace renderer
} // namespace nkentseu
