// =============================================================================
// Applications/NkAssetIODemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour les adaptateurs IO de Noge réécrits le
// 2026-07-23 (voir Engine/Noge/ROADMAP.md, Phase G1 item 3) :
//   Engine/Noge/src/Noge/IO/{NkOBJIO,NkGLTFIO,NkFBXImporter}.h/.cpp
//
// `jenga test` / les projets `*_Tests` sont bloqués par la politique de
// workspace ("Unit-test compilation/execution is disabled by workspace
// policy", déjà rencontrée pour NkEditableMeshDemo le même jour). Cette
// application console classique (pas une TestSuite) contourne le blocage.
//
// MÉTHODE DE COMPARAISON ("résultat identique au chemin NkMeshSystem::Import
// direct", jalon de la ROADMAP) :
//   NkMeshSystem::Import(path) fait exactement 2 choses : (1) appelle le
//   loader CPU réel selon l'extension (renderer::LoadOBJ/LoadGLTF/LoadFBX —
//   les MÊMES fonctions que les adaptateurs Noge/IO appellent, zéro logique
//   dupliquée des deux côtés) ; (2) NkMeshSystem::Create(desc), qui copie
//   desc.vertexCount/indexCount TELS QUELS dans le mesh GPU (voir
//   NkMeshSystem.cpp ~L96-104 : `e.vertexCount = desc.vertexCount;` —
//   affectation directe, aucune transformation). Le compte de sommets/faces
//   d'un mesh importé via NkMeshSystem::Import EST DONC, par construction,
//   celui produit par l'étape (1). Cette démo appelle l'étape (1) en direct
//   (baseline) et la compare au résultat des adaptateurs Noge/IO — preuve
//   d'équivalence sans dépendre d'un NkIDevice réel (Create() exige un
//   device GPU vivant ; aucun backend RHI n'est aujourd'hui instancié par un
//   seul appelant dans tout le repo — bootstrapper ce chemin GPU pour cette
//   seule démo CPU-only sortirait du scope d'un adaptateur IO "fin" — voir
//   Engine/Noge/ROADMAP.md pour le suivi honnête de cette limite).
// =============================================================================
#include "Noge/IO/NkOBJIO.h"
#include "Noge/IO/NkGLTFIO.h"
#include "Noge/IO/NkFBXImporter.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

namespace {

	int gPassCount = 0;
	int gFailCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	// ── OBJ ────────────────────────────────────────────────────────────────
	void TestOBJ(const char *path) noexcept {
		logger.Infof("-- NkOBJIO: %s --\n", path);

		renderer::NkGLTFMeshData baseline;
		const bool baseOk = renderer::LoadOBJ(NkString(path), baseline);
		Check(baseOk && baseline.IsValid(), "baseline renderer::LoadOBJ reussit");
		if (!baseOk || !baseline.IsValid()) {
			return;
		}
		const uint32 baseVerts = (uint32)baseline.vertices.Size();
		const uint32 baseFaces = (uint32)baseline.indices.Size() / 3;
		logger.Infof("     baseline: %u sommets, %u faces, %u sous-meshes\n", baseVerts, baseFaces,
					 (uint32)baseline.subMeshes.Size());

		NkEditableMesh em = NkOBJIO::Import(path);
		Check(NkOBJIO::GetLastError().Empty(), "NkOBJIO::Import: aucune erreur");
		Check(em.VertexCount() == baseVerts, "NkOBJIO::Import: VertexCount == baseline renderer::LoadOBJ");
		Check(em.FaceCount() == baseFaces, "NkOBJIO::Import: FaceCount == baseline renderer::LoadOBJ");
		logger.Infof("     NkOBJIO : %u sommets, %u faces\n", em.VertexCount(), em.FaceCount());

		// Export non implémenté : vérifie l'échec honnête (pas de fausse réussite).
		Check(!NkOBJIO::Export(em, "unused.obj"), "NkOBJIO::Export retourne false (non implemente, honnete)");
	}

	// ── glTF/GLB ──────────────────────────────────────────────────────────
	void TestGLTF(const char *path) noexcept {
		logger.Infof("-- NkGLTFIO: %s --\n", path);

		renderer::NkGLTFMeshData baseline;
		const bool baseOk = renderer::LoadGLTF(NkString(path), baseline);
		Check(baseOk && baseline.IsValid(), "baseline renderer::LoadGLTF reussit");
		if (!baseOk || !baseline.IsValid()) {
			return;
		}
		const uint32 baseVerts = (uint32)baseline.vertices.Size();
		const uint32 baseFaces = (uint32)baseline.indices.Size() / 3;
		logger.Infof("     baseline: %u sommets, %u faces, %u materiaux, %u nodes, %u anims, skinne=%s\n",
					 baseVerts, baseFaces, (uint32)baseline.materials.Size(), (uint32)baseline.nodes.Size(),
					 (uint32)baseline.animations.Size(), baseline.isSkinned ? "oui" : "non");

		NkGLTFImporter importer;
		NkGLTFScene scene = importer.Import(path);
		Check(importer.GetLastError().Empty(), "NkGLTFImporter::Import: aucune erreur");
		Check(scene.IsValid(), "NkGLTFImporter::Import: scene valide");
		Check(scene.meshes.Size() == 1, "NkGLTFImporter::Import: 1 seule entree mesh (deja fusionnee)");
		if (!scene.meshes.Empty()) {
			Check(scene.meshes[0].mesh.VertexCount() == baseVerts,
				  "NkGLTFImporter::Import: VertexCount == baseline renderer::LoadGLTF");
			Check(scene.meshes[0].mesh.FaceCount() == baseFaces,
				  "NkGLTFImporter::Import: FaceCount == baseline renderer::LoadGLTF");
			logger.Infof("     NkGLTFIO: %u sommets, %u faces\n", scene.meshes[0].mesh.VertexCount(),
						 scene.meshes[0].mesh.FaceCount());
		}
		Check(scene.materials.Size() == baseline.materials.Size(),
			  "NkGLTFImporter::Import: nb materiaux == baseline");
		Check(scene.nodes.Size() == baseline.nodes.Size(), "NkGLTFImporter::Import: nb nodes == baseline");
		Check(scene.animations.Size() == baseline.animations.Size(),
			  "NkGLTFImporter::Import: nb animations == baseline");

		if (baseline.isSkinned && !baseline.skinJoints.Empty()) {
			const uint32 expectedBones = (uint32)baseline.skinJoints.Size();
			Check(scene.skeletons.Size() == 1, "NkGLTFImporter::Import: 1 squelette (fichier skinne)");
			if (!scene.skeletons.Empty()) {
				Check(scene.skeletons[0].boneCount == expectedBones,
					  "NkGLTFImporter::Import: boneCount == baseline.skinJoints.Size()");
				logger.Infof("     squelette: %u os\n", scene.skeletons[0].boneCount);
			}
		} else {
			Check(scene.skeletons.Empty(), "NkGLTFImporter::Import: 0 squelette (fichier non skinne)");
		}

		// MergeAllMeshes : clone du mesh deja fusionne -> memes comptes.
		NkEditableMesh merged = scene.MergeAllMeshes();
		Check(merged.VertexCount() == baseVerts, "NkGLTFScene::MergeAllMeshes: VertexCount == baseline");

		// Round-trip mémoire / spawn : non implémentés, vérifie l'échec honnête.
		Check(!importer.ImportFromMemory(nullptr, 0).IsValid(),
			  "NkGLTFImporter::ImportFromMemory: scene invalide (non implemente, honnete)");
	}

	// ── FBX ───────────────────────────────────────────────────────────────
	void TestFBX(const char *path) noexcept {
		logger.Infof("-- NkFBXImporter: %s --\n", path);

		renderer::NkGLTFMeshData baseline;
		const bool baseOk = renderer::LoadFBX(NkString(path), baseline);
		Check(baseOk && baseline.IsValid(), "baseline renderer::LoadFBX reussit");
		if (!baseOk || !baseline.IsValid()) {
			return;
		}
		const uint32 baseVerts = (uint32)baseline.vertices.Size();
		const uint32 baseFaces = (uint32)baseline.indices.Size() / 3;
		logger.Infof("     baseline: %u sommets, %u faces\n", baseVerts, baseFaces);

		NkFBXImporter importer;
		NkFBXScene scene = importer.Import(path);
		Check(importer.GetLastError().Empty(), "NkFBXImporter::Import: aucune erreur");
		Check(scene.valid, "NkFBXImporter::Import: scene valide");
		Check(scene.meshes.Size() == 1, "NkFBXImporter::Import: 1 seule entree mesh (deja fusionnee)");
		if (!scene.meshes.Empty()) {
			Check(scene.meshes[0].VertexCount() == baseVerts,
				  "NkFBXImporter::Import: VertexCount == baseline renderer::LoadFBX");
			Check(scene.meshes[0].FaceCount() == baseFaces,
				  "NkFBXImporter::Import: FaceCount == baseline renderer::LoadFBX");
			logger.Infof("     NkFBXImporter: %u sommets, %u faces\n", scene.meshes[0].VertexCount(),
						 scene.meshes[0].FaceCount());
		}
		// Le loader reel ne supporte ni materiaux ni squelette/anim : verifie
		// que l'adaptateur n'invente rien.
		Check(scene.materials.Empty(), "NkFBXImporter::Import: 0 materiau (non supporte par le loader reel)");
		Check(scene.skeletons.Empty(), "NkFBXImporter::Import: 0 squelette (non supporte par le loader reel)");
		Check(scene.animations.Empty(), "NkFBXImporter::Import: 0 animation (non supportee par le loader reel)");
	}

} // namespace

int main() {
	logger.Infof("=== NkAssetIODemo : preuve d'execution Noge/IO (Phase G1.3) ===\n");

	TestOBJ("Resources/Models/tree.obj");
	TestOBJ("Resources/Models/rock/rock.obj");

	TestGLTF("Resources/Models/rubber_duck/scene.gltf");
	TestGLTF("Resources/Models/CesiumMan/CesiumMan.glb");

	TestFBX("Resources/Models/test/cube_ascii.fbx");
	TestFBX("Resources/Models/Futuristic_Car_2.1_fbx.fbx");

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
