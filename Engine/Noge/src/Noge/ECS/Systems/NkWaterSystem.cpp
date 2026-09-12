// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Systems/NkWaterSystem.cpp — pont ECS -> producteur d'eau. Voir
// l'en-tête pour l'idiome suivi, la caméra, l'espace et les bornes.
// =============================================================================
#include "NkWaterSystem.h"
#include "Noge/ECS/Components/Core/NkTag.h" // NkInactive
#include "NKRenderer/Mesh/NkMeshSystem.h"

namespace nkentseu {

	using namespace ecs;
	using namespace renderer;
	using namespace math;

	void NkWaterSystem::Execute(NkWorld &world, float32 dt) noexcept {
		mBuilt = 0;
		mUploaded = 0;
		mHidden = 0;

		// 1. La caméra active — même règle que NkRenderSystem::UpdateActiveCamera :
		//    priorité maximale, NkInactive exclu. Recalculée ici, voir l'en-tête.
		bool camOk = false;
		int32 best = -99999;
		NkMat4f proj = NkMat4f::Identity(), view = NkMat4f::Identity();
		NkVec3f eye = {0.f, 0.f, 0.f};
		world.Query<NkCameraComponent, NkTransform>().ForEach(
			[&](NkEntityId id, const NkCameraComponent &cam, const NkTransform &tf) {
				if (world.Has<NkInactive>(id))
					return;
				if (cam.priority <= best)
					return;
				if (cam.projection != NkCameraProjection::Perspective)
					return; // la grille projetée n'a pas de sens en orthographique
				best = cam.priority;
				const NkVec3f pos = tf.GetWorldPosition();
				const NkVec3f fwd = tf.GetWorldForward();
				const NkVec3f up = tf.GetWorldUp();
				const float32 aspect = (cam.aspect > 0.f) ? cam.aspect : (16.f / 9.f);
				view = NkMat4f::LookAt(pos, pos + fwd, up);
				proj = NkMat4f::Perspective(math::NkAngle(cam.fovDeg), aspect, cam.nearClip, cam.farClip);
				eye = pos;
				camOk = true;
			});
		if (!camOk) {
			++mNoCamera;
			return; // sans caméra il n'y a pas de grille : on ne fabrique rien
		}

		// 2. Chaque surface d'eau.
		world.Query<NkWaterComponent, NkMeshComponent, NkTransform>().ForEach(
			[&](NkEntityId id, NkWaterComponent &w, NkMeshComponent &mesh, const NkTransform &tf) {
				if (world.Has<NkInactive>(id) || !w.enabled) {
					mesh.visible = false;
					++mHidden;
					return;
				}
				w.time += dt;

				const NkVec3f origin = tf.GetWorldPosition();
				vfx::NkWaterMeshParams p;
				p.grid = w.grid;
				p.grid.baseY = origin.y; // le plan de repos EST la position de l'entité
				p.waves = w.waves;
				p.time = w.time;
				p.color = w.color;

				const uint32 need = vfx::NkWaterVertexCount(p.grid);
				if (need == 0u) {
					mesh.visible = false;
					++mHidden;
					return;
				}
				if (mVertices.Size() < need)
					mVertices.Resize(need);

				// L'ÉTAGE CPU : tourne avec ou sans device. C'est lui que la sonde
				// chronomètre ; sans cette ligne, rien ne prouverait que le système
				// s'exécute et pas seulement qu'il est enregistré.
				uint32 missing = 0u;
				const uint32 n = vfx::NkWaterBuildVertices(proj, view, eye, p, mVertices.Data(),
														   (uint32)mVertices.Size(), &missing);
				w.lastVertexCount = n;
				w.lastMissing = missing;
				if (n == 0u) {
					mesh.visible = false; // grille hors champ, ou refus : la grille l'a DIT
					++mHidden;
					return;
				}
				mBuilt += n;

				// Sommets MONDE -> LOCAUX : NkRenderSystem applique worldMatrix.
				// Translation seule, voir l'en-tête.
				for (uint32 k = 0; k < n; ++k)
					mVertices[k].pos = mVertices[k].pos - origin;

				if (mMeshes == nullptr)
					return; // sans registre : le CPU a tourné, rien à téléverser

				// L'ÉTAGE GPU. Recréation si la grille a changé de taille : un tampon
				// dynamique a une taille fixe, on n'y écrit pas plus qu'il ne tient.
				if (w.meshHandle != 0ull && (w.createdCols != p.grid.cols || w.createdRows != p.grid.rows)) {
					NkMeshHandle old;
					old.id = w.meshHandle;
					mMeshes->Release(old);
					w.meshHandle = 0ull;
				}
				if (w.meshHandle == 0ull) {
					const uint32 ni = vfx::NkWaterIndexCount(p.grid);
					if (mIndices.Size() < ni)
						mIndices.Resize(ni);
					const uint32 written = vfx::NkWaterBuildIndices(p.grid, mIndices.Data(), (uint32)mIndices.Size());
					if (written != ni) {
						mesh.visible = false; // le pavage a refusé : pas de maillage à trous
						++mHidden;
						return;
					}
					// `renderer::` explicite : un homonyme NkVertexLayout vit hors de ce
					// namespace, et `using namespace renderer` ne suffit pas à trancher.
					renderer::NkMeshDesc desc = renderer::NkMeshDesc::Simple(
						renderer::NkVertexLayout::Default3D(), mVertices.Data(), n, mIndices.Data(), ni);
					desc.dynamic = true;
					desc.keepCPU = false; // le CPU regénère tout à chaque image : pas de copie
					desc.bounds.min = {-1.0e6f, -1.0e4f, -1.0e6f}; // immenses : voir l'en-tête
					desc.bounds.max = {1.0e6f, 1.0e4f, 1.0e6f};
					desc.debugName = "NkWater";
					const NkMeshHandle h = mMeshes->Create(desc);
					if (!h.IsValid()) {
						mesh.visible = false;
						++mHidden;
						return; // le registre a refusé et l'a dit : pas de poignée inventée
					}
					w.meshHandle = h.id;
					w.createdCols = p.grid.cols;
					w.createdRows = p.grid.rows;
					++mCreated;
				}

				mesh.meshHandle = w.meshHandle;
				NkMeshHandle h;
				h.id = w.meshHandle;
				if (mMeshes->UpdateVertices(h, mVertices.Data(), n)) {
					mesh.visible = true;
					++mUploaded;
				} else {
					mesh.visible = false;
					++mHidden;
				}
			});
	}

} // namespace nkentseu
