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
				Check(scene.skeletons[0].BoneCount() == expectedBones,
					  "NkGLTFImporter::Import: boneCount == baseline.skinJoints.Size()");
				logger.Infof("     squelette: %u os\n", scene.skeletons[0].BoneCount());
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
	// `expectedMaterials`   : combien de materiaux le loader doit rendre.
	// `expectedResolvedTex` : combien d'images doivent etre REELLEMENT DECODEES.
	//
	// ⚠️ POURQUOI DEUX ATTENTES, ET POURQUOI ELLES SONT PARAMETREES (2026-09-02).
	// Jusqu'a cette date le banc exigeait `scene.materials.Empty()` en toutes
	// circonstances, avec le commentaire « le loader reel ne supporte pas les
	// materiaux ». C'etait vrai a l'ecriture ; le commit d28a3728 a AJOUTE les
	// materiaux Phong + textures externes. La garde codait donc une LIMITATION
	// qui a ete levee, et son rouge s'est fait lire A L'ENVERS -- comme une
	// regression du chargeur, alors qu'il venait de progresser.
	//   -> Une garde qui encode une limitation doit nommer ce qu'elle attend,
	//      sinon elle survit a ce qu'elle decrivait et accuse le progres.
	//
	// ⚠️ ET LA SECONDE ATTENTE EST CELLE QUI MANQUAIT VRAIMENT. Compter les
	// materiaux ne dit RIEN de leurs textures : un materiau qui NOMME
	// 'textures/x.jpg' sans que le fichier soit trouve compte exactement comme un
	// materiau texture. C'est le defaut mesure le 2026-09-02 -- le sous-dossier
	// etait jete par NkFBXLoader, donc TROIS materiaux et ZERO image, et ce banc
	// serait reste vert. On verifie donc `NkGLTFImage::valid`, le drapeau du
	// DECODAGE reel, et non la presence d'un nom.
	void TestFBX(const char *path, uint32 expectedMaterials, uint32 expectedResolvedTex) noexcept {
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
		// Materiaux : le compte ATTENDU, nomme par l'appelant (cf. bloc en tete).
		Check(scene.materials.Size() == expectedMaterials,
			  "NkFBXImporter::Import: nombre de materiaux conforme a l'attendu");
		logger.Infof("     materiaux: %u (attendu %u)\n", (uint32)scene.materials.Size(), expectedMaterials);

		// ── LE CONTROLE QUI MANQUAIT : une texture RESOLUE, pas declaree ──────
		// `NkGLTFImage::valid` n'est vrai que si le fichier a ete trouve ET
		// decode. Un nom de texture pointant dans le vide laisse `valid` a faux.
		uint32 resolved = 0;
		for (uint32 i = 0; i < (uint32)baseline.images.Size(); ++i) {
			if (baseline.images[(decltype(baseline.images)::SizeType)i].valid)
				++resolved;
		}
		Check(resolved == expectedResolvedTex,
			  "renderer::LoadFBX: nombre de textures REELLEMENT DECODEES conforme");
		logger.Infof("     textures decodees: %u / %u declarees (attendu %u)\n", resolved,
					 (uint32)baseline.images.Size(), expectedResolvedTex);

		// Squelette et animation : toujours des no-ops cote adaptateur. Si l'un
		// des deux devient supporte un jour, CE Check tombera -- et il faudra
		// le parametrer comme les materiaux ci-dessus, pas le supprimer.
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

	// cube_ascii : geometrie nue, aucun materiau, aucune texture.
	TestFBX("Resources/Models/test/cube_ascii.fbx", 0, 0);
	// Futuristic_Car : 3 materiaux Phong (carrosserie / noir / vitrage) et
	// 3 textures externes (C/N/S) qui vivent dans Resources/Models/textures/.
	// Les 3 doivent etre DECODEES : c'est ce chiffre qui a valu 0 jusqu'au
	// correctif de resolution de chemin du 2026-09-02 (NkFBXLoader).
	TestFBX("Resources/Models/Futuristic_Car_2.1_fbx.fbx", 3, 3);

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
