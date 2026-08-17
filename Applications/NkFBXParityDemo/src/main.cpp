// =============================================================================
// Applications/NkFBXParityDemo/src/main.cpp
// =============================================================================
// Banc de parite FBX / glTF — chantier « FBX operationnel » (2026-08-17).
// Voir NkFBXParityDemo.jenga pour le contexte et la provenance des chiffres
// de reference. Lancer depuis la RACINE du worktree.
//
// Deux roles :
//   1. TEMOIN ADDITIF : le chemin glTF (CesiumMan.glb) ne doit pas bouger —
//      ses invariants (19 joints, skinne, animations) sont verifies a chaque
//      course de ce banc.
//   2. PREUVE FBX par etape : (a) squelette + noms (nodes, hierarchie, TRS
//      contre l'inspecteur independant) ; (b) skinning et (c) animations
//      viendront s'ajouter ici.
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKLogger/NkLog.h"

#include <cmath>

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

	int32 FindNodeByName(const renderer::NkGLTFMeshData &d, const char *name) noexcept {
		for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i)
			if (d.nodes[i].name == NkString(name))
				return (int32)i;
		return -1;
	}

	// ── Reference glTF : le chemin prouve, qui ne doit pas bouger ──────────
	void TestGLTFReference(renderer::NkGLTFMeshData &gltf) noexcept {
		logger.Infof("-- Reference glTF : CesiumMan.glb --\n");
		const bool ok = renderer::LoadGLTF(NkString("Resources/Models/CesiumMan/CesiumMan.glb"), gltf);
		Check(ok && gltf.IsValid(), "LoadGLTF CesiumMan.glb reussit");
		if (!ok)
			return;
		Check(gltf.isSkinned, "glTF : isSkinned");
		Check(gltf.skinJoints.Size() == 19, "glTF : 19 joints (skins[0].joints)");
		Check(gltf.inverseBind.Size() == 19, "glTF : 19 inverseBind");
		Check(gltf.animations.Size() == 1, "glTF : 1 animation");
		Check(gltf.skinnedVertices.Size() == gltf.vertices.Size(),
			  "glTF : skinnedVertices parallele a vertices");
		logger.Infof("     glTF : %u verts, %u nodes, %u joints, %u animations, duree %.3fs\n",
					 (uint32)gltf.vertices.Size(), (uint32)gltf.nodes.Size(), (uint32)gltf.skinJoints.Size(),
					 (uint32)gltf.animations.Size(),
					 gltf.animations.Empty() ? 0.f : gltf.animations[0].duration);
	}

	// ── Etape (a) : squelette + noms depuis le FBX ─────────────────────────
	void TestFBXNodes(renderer::NkGLTFMeshData &fbx) noexcept {
		logger.Infof("-- FBX etape (a) : CesiumMan.fbx (squelette + noms) --\n");
		const bool ok = renderer::LoadFBX(NkString("Resources/Models/CesiumMan/CesiumMan.fbx"), fbx);
		Check(ok && fbx.IsValid(), "LoadFBX CesiumMan.fbx reussit");
		if (!ok)
			return;

		// Chiffres de l'inspecteur independant : 23 Model, hierarchie a 20
		// aretes (23 - 3 racines).
		Check(fbx.nodes.Size() == 23, "FBX : 23 nodes (23 Model — inspecteur independant)");
		uint32 named = 0, edges = 0;
		for (uint32 i = 0; i < (uint32)fbx.nodes.Size(); ++i) {
			if (!fbx.nodes[i].name.Empty())
				++named;
			edges += (uint32)fbx.nodes[i].children.Size();
		}
		Check(named == fbx.nodes.Size(), "FBX : tous les nodes ont un nom");
		Check(edges == 20, "FBX : 20 aretes de hierarchie (23 nodes - 3 racines)");

		// Le premier joint du squelette, par son NOM (ce que glTF ne sait pas
		// faire aujourd'hui), et sa translation lue des Properties70 —
		// valeurs de l'inspecteur : (1.9558e-08, -0.6439971, -0.0200001).
		const int32 j1 = FindNodeByName(fbx, "Skeleton_torso_joint_1");
		Check(j1 >= 0, "FBX : node 'Skeleton_torso_joint_1' trouve par son nom");
		if (j1 >= 0) {
			const renderer::NkGLTFNode &n = fbx.nodes[(uint32)j1];
			const bool tOk = std::fabs(n.translation.y - (-0.6439971f)) < 1e-4f &&
							 std::fabs(n.translation.z - (-0.0200001f)) < 1e-4f;
			Check(tOk, "FBX : Lcl Translation de torso_joint_1 == inspecteur (P70 lues)");
			const float32 qlen = n.rotation.x * n.rotation.x + n.rotation.y * n.rotation.y +
								 n.rotation.z * n.rotation.z + n.rotation.w * n.rotation.w;
			Check(std::fabs(qlen - 1.f) < 1e-3f, "FBX : quaternion de rotation normalise");
			// Chaine du squelette : torso_joint_1 doit avoir au moins un enfant
			// (torso_joint_2) — la hierarchie traverse les connexions OO.
			Check(!n.children.Empty(), "FBX : torso_joint_1 a des enfants (chaine du squelette)");
		}

		// La geometrie n'a pas bouge : memes invariants qu'avant l'etape (a).
		Check(!fbx.vertices.Empty() && !fbx.indices.Empty(), "FBX : geometrie toujours chargee");
		logger.Infof("     FBX : %u verts, %u nodes, %u sous-meshes\n", (uint32)fbx.vertices.Size(),
					 (uint32)fbx.nodes.Size(), (uint32)fbx.subMeshes.Size());
	}

	// ── Etape (b) : skinning depuis le FBX ─────────────────────────────────
	// References de l'inspecteur Python INDEPENDANT du moteur (fbx_skin_inspect,
	// 2026-08-17) sur CesiumMan.fbx : 1 Skin sur la Geometry aux 3273 control
	// points (14016 coins emis sur 14256), 19 Cluster relies chacun a un Model
	// LimbNode nomme, couverture 3273/3273, somme des poids exactement 1, max
	// 4 influences, 86 % des control points a >= 2 influences.
	// inverseBind(Skeleton_torso_joint_1) = TransformLink^-1 * Transform,
	// translation column-major [12..14] = (0.044553, -0.004487, -0.677621).
	// NOTE parite : les inverseBind glTF et FBX ne sont PAS comparables
	// directement — l'export Blender integre l'echelle cm et sa reorientation
	// des os dans TransformLink/Transform (glTF : t=(0.0513, -0.0050, -0.6771)).
	// Le temoin numerique est donc l'inspecteur, sur le MEME fichier.
	void TestFBXSkin(const renderer::NkGLTFMeshData &fbx) noexcept {
		logger.Infof("-- FBX etape (b) : CesiumMan.fbx (skinning) --\n");
		Check(fbx.isSkinned, "FBX : isSkinned (le Deformer Skin est extrait)");
		Check(fbx.skinJoints.Size() == 19, "FBX : 19 joints (19 Cluster — inspecteur independant)");
		Check(fbx.inverseBind.Size() == fbx.skinJoints.Size(), "FBX : inverseBind parallele a skinJoints");
		Check(fbx.skinnedVertices.Size() == fbx.vertices.Size(),
			  "FBX : skinnedVertices parallele a vertices");
		if (!fbx.isSkinned || fbx.skinJoints.Empty())
			return;

		// Chaque joint est un node valide et NOMME (les Cluster nomment leurs
		// joints — ce que le chemin glTF ne garde pas).
		bool jointsOk = true;
		int32 torso = -1;
		for (uint32 k = 0; k < (uint32)fbx.skinJoints.Size(); ++k) {
			const int32 n = fbx.skinJoints[k];
			if (n < 0 || n >= (int32)fbx.nodes.Size() || fbx.nodes[(uint32)n].name.Empty()) {
				jointsOk = false;
				break;
			}
			if (fbx.nodes[(uint32)n].name == NkString("Skeleton_torso_joint_1"))
				torso = (int32)k;
		}
		Check(jointsOk, "FBX : chaque skinJoint est un node valide et nomme");
		Check(torso >= 0, "FBX : 'Skeleton_torso_joint_1' figure parmi les joints");

		// La matrice de bind du joint teste, contre l'inspecteur : verifie d'un
		// coup Inverse(), le produit et la convention column-major du moteur.
		if (torso >= 0) {
			const math::NkMat4f &ib = fbx.inverseBind[(uint32)torso];
			const bool tOk = std::fabs(ib.data[12] - 0.044553f) < 1e-3f &&
							 std::fabs(ib.data[13] - (-0.004487f)) < 1e-3f &&
							 std::fabs(ib.data[14] - (-0.677621f)) < 1e-3f;
			Check(tOk, "FBX : inverseBind(torso_joint_1).translation == TL^-1*TR de l'inspecteur");
		}

		// Poids : somme 1 partout (sommets skinnes normalises, hors-skin au
		// defaut {1,0,0,0} — meme regle que le chemin glTF), indices bornes,
		// et la couverture est reelle (>= 50 % de sommets multi-influences ;
		// mesure independante : 86 % des control points, 98 % des coins emis
		// viennent de la geometrie skinnee).
		bool sumOk = true, idxOk = true;
		uint32 multi = 0;
		for (uint32 v = 0; v < (uint32)fbx.skinnedVertices.Size(); ++v) {
			const renderer::NkVertexSkinned &sv = fbx.skinnedVertices[v];
			float32 s = 0.f;
			uint32 used = 0;
			for (uint32 k = 0; k < 4; ++k) {
				s += sv.boneWeight[k];
				if (sv.boneWeight[k] > 0.f)
					++used;
				if (sv.boneIdx[k] < 0.f || sv.boneIdx[k] >= (float32)fbx.skinJoints.Size())
					idxOk = false;
			}
			if (std::fabs(s - 1.f) > 1e-3f)
				sumOk = false;
			if (used >= 2)
				++multi;
		}
		Check(sumOk, "FBX : somme des poids == 1 pour chaque sommet (skinnes et defaut)");
		Check(idxOk, "FBX : boneIdx borne par le nombre de joints");
		Check(multi * 2 >= (uint32)fbx.skinnedVertices.Size(),
			  "FBX : >= 50 % des sommets ont >= 2 influences (couverture reelle)");
		logger.Infof("     FBX skin : %u joints, %u/%u sommets multi-influences\n",
					 (uint32)fbx.skinJoints.Size(), multi, (uint32)fbx.skinnedVertices.Size());
	}

	// ── Non-regression ASCII : le cube de test (aucun Model dedans) ────────
	void TestFBXAscii() noexcept {
		logger.Infof("-- FBX ASCII : cube_ascii.fbx (non-regression geometrie) --\n");
		renderer::NkGLTFMeshData cube;
		const bool ok = renderer::LoadFBX(NkString("Resources/Models/test/cube_ascii.fbx"), cube);
		Check(ok && cube.IsValid(), "LoadFBX cube_ascii.fbx reussit toujours");
		if (ok)
			Check(cube.nodes.Empty(), "FBX ASCII : 0 node (le fichier n'a aucun Model — zero invente)");
	}

} // namespace

int main() {
	logger.Infof("=== NkFBXParityDemo — parite FBX / glTF (CesiumMan) ===\n");

	renderer::NkGLTFMeshData gltf; // non copiable : vit ici, passe par reference
	renderer::NkGLTFMeshData fbx;
	TestGLTFReference(gltf);
	TestFBXNodes(fbx);
	TestFBXSkin(fbx);
	TestFBXAscii();

	// Parite inter-chemins (etape (a) : ce qui est deja comparable).
	if (gltf.IsValid() && fbx.IsValid()) {
		logger.Infof("-- Parite glTF <-> FBX --\n");
		// 19 joints glTF <-> 19 LimbNode parmi les 23 Model FBX. L'etape (b)
		// comparera joint a joint ; ici on verifie que le squelette FBX
		// contient AU MOINS autant de nodes que le squelette glTF a de joints.
		Check(fbx.nodes.Size() >= gltf.skinJoints.Size(),
			  "parite : nodes FBX >= joints glTF (le squelette est dans l'arbre)");
		// Etape (b) : les deux chemins voient LE MEME squelette de 19 joints.
		Check(fbx.skinJoints.Size() == gltf.skinJoints.Size(),
			  "parite : autant de joints skin FBX que glTF (19 Cluster = 19 joints)");
	}

	logger.Infof("=== Resultat : %d OK, %d echec(s) ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
