#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Systems/NkWaterSystem.h — pont ECS -> producteur d'eau
// (vfx::NkWaterBuildVertices), 2026-09-12.
//
// LOCATAIRE MINCE. Pour chaque entité (NkWaterComponent + NkMeshComponent +
// NkTransform) il demande au producteur les sommets de CETTE image depuis la
// caméra active, les téléverse dans un maillage DYNAMIQUE qu'il a créé une
// fois, et écrit la poignée dans `NkMeshComponent`. `NkRenderSystem` dessine.
// RIEN de la logique d'eau ne descend ici : le producteur ne sait pas qu'il a un
// hôte, et cet hôte ne sait rien de la houle.
//
// ── ⚠️ L'IDIOME SUIVI, ET POURQUOI ────────────────────────────────────────────
// Deux idiomes d'adaptateur coexistent dans Noge :
//   * `NkParticleSystem` prend un `renderer::NkRenderer*` — aucun banc ne peut
//     l'exercer sans device, et personne ne l'a jamais fait tourner sans fenêtre ;
//   * `NkFluidVolumeSystem` (2026-09-06) prend le REGISTRE, la dépendance la
//     plus ÉTROITE, et s'éprouve headless — son en-tête dit que « ce n'est pas
//     un détail de style ».
// CE SYSTÈME SUIT LE SECOND : il prend un `renderer::NkMeshSystem*` — la seule
// chose dont il ait besoin — et NON le renderer. Et il va un cran plus loin :
// SANS lui (nullptr), l'étage CPU tourne quand même. C'est ce qui permet à la
// sonde `NkEauEcsProbe` de mesurer sa VIE sans device — le seul témoin qui
// sépare « enregistré » d'« exécuté ». Un troisième idiome, c'est nous qui
// l'aurions créé.
//
// ── LA CAMÉRA ─────────────────────────────────────────────────────────────────
// `NkRenderSystem` calcule view/proj dans le groupe Render, APRÈS ce système ;
// lire ses matrices serait lire l'IMAGE PRÉCÉDENTE — et rien du tout sans
// device. Ce système les recalcule donc depuis la caméra de priorité maximale,
// avec les MÊMES appels (LookAt / Perspective). DETTE NOMMÉE : ces huit lignes
// existent désormais deux fois ; `NkRenderSystem` devrait appeler une fonction
// commune. Non fait ce soir : c'est un fichier partagé avec le chantier tissu.
// Une caméra orthographique est IGNORÉE : la grille projetée n'y a pas de sens.
//
// ── L'ESPACE ──────────────────────────────────────────────────────────────────
// Le producteur rend des sommets MONDE, sur le plan y = position monde de
// l'entité. `NkRenderSystem` applique `worldMatrix` au dessin ; on retranche
// donc la position pour livrer des sommets LOCAUX. TRANSLATION SEULE : une
// entité d'eau tournée ou mise à l'échelle n'est pas prise en charge — nommé,
// pas géré.
//
// ── LES BORNES ────────────────────────────────────────────────────────────────
// Le maillage suit la caméra ; ses bornes changent à chaque image, et
// `NkMeshSystem` n'a pas de SetBounds. On déclare des bornes IMMENSES à la
// création : le culling de `NkRenderSystem` ne coupe jamais. C'est la grille
// (`visible`) qui décide, et `NkMeshComponent::visible` la suit.
// =============================================================================
#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Components/Rendering/NkWaterComponent.h"
#include "NKVFX/NkWaterMeshBuilder.h" // NkVertex3D complet, pour le tampon
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {
		class NkMeshSystem;
	}

	class NkWaterSystem final : public ecs::NkSystem {
		public:
			NkWaterSystem() noexcept = default;

			// Le registre de maillages est EMPRUNTÉ (au renderer de l'application) et
			// doit survivre à ce système. nullptr est un état LÉGITIME : l'étage CPU
			// tourne, rien n'est téléversé — c'est ainsi que la sonde l'éprouve.
			void Init(renderer::NkMeshSystem *meshes) noexcept {
				mMeshes = meshes;
			}

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkWaterComponent>()
					.Writes<ecs::NkMeshComponent>()
					.Reads<ecs::NkTransform>()
					.Reads<ecs::NkCameraComponent>()
					.InGroup(ecs::NkSystemGroup::PostUpdate)
					.Sequential()
					.Named("NkWaterSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

			// ── Témoins — comptés APRÈS le geste, jamais avant ─────────────────────
			uint32 BuiltLastFrame() const noexcept { // sommets produits par le CPU
				return mBuilt;
			}
			uint32 UploadedLastFrame() const noexcept { // maillages téléversés au GPU
				return mUploaded;
			}
			uint32 HiddenLastFrame() const noexcept { // entités laissées invisibles
				return mHidden;
			}
			uint32 CreatedTotal() const noexcept { // maillages créés depuis le début
				return mCreated;
			}
			uint32 FramesWithoutCamera() const noexcept {
				return mNoCamera;
			}
			// Les sommets LOCAUX de la dernière surface produite (BuiltLastFrame()
			// d'entre eux sont valides). Lecture seule : une sonde s'en sert pour
			// vérifier que le plan suit l'entité sans device.
			const renderer::NkVertex3D *LastVertices() const noexcept {
				return mVertices.Data();
			}

		private:
			renderer::NkMeshSystem *mMeshes = nullptr; // EMPRUNTÉ
			NkVector<renderer::NkVertex3D> mVertices;  // tampon réutilisé, 0 allocation en régime
			NkVector<uint32> mIndices;				   // idem, pour la création
			uint32 mBuilt = 0, mUploaded = 0, mHidden = 0, mCreated = 0, mNoCamera = 0;
	};

} // namespace nkentseu
