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

	// ── Etape (c) : animations depuis le FBX ───────────────────────────────
	// References de l'inspecteur independant : 1 AnimationStack « Scene »,
	// 57 CurveNode (19 joints x T/R/S), 171 courbes d|X/Y/Z, cles de 0.041667 s
	// (frame 1 a 24 fps) a 10.416667 s (frame 250 — l'export Blender ETALE la
	// timeline de scene : la parite de duree avec le glTF, 2.0 s, est IMPOSSIBLE
	// sur ce temoin ; c'est l'inspecteur qui fait foi, comme pour inverseBind).
	// Canal R de torso_joint_1, cle 0 : euler (90.005852, -0.002983, 4.200809)
	// XYZ sans PreRotation -> quat (0.706668, 0.025899, 0.025933, 0.706595).
	void TestFBXAnim(const renderer::NkGLTFMeshData &fbx) noexcept {
		logger.Infof("-- FBX etape (c) : CesiumMan.fbx (animations) --\n");
		Check(fbx.animations.Size() == 1, "FBX : 1 animation (1 AnimationStack)");
		if (fbx.animations.Size() != 1)
			return;
		const renderer::NkGLTFAnimation &a = fbx.animations[0];
		Check(a.name == NkString("Scene"), "FBX : l'animation porte le nom du Stack ('Scene')");
		Check(a.channels.Size() == 57, "FBX : 57 canaux (57 CurveNode = 19 joints x T/R/S)");
		Check(std::fabs(a.duration - 10.416667f) < 1e-3f,
			  "FBX : duree 10.416667 s (derniere cle — inspecteur, pas de rebasage)");

		// Chaque canal : cible valide, temps strictement croissants depuis la
		// frame 1, quaternions normalises sur les canaux ROTATION.
		bool nodesOk = true, timesOk = true, quatsOk = true;
		for (uint32 c = 0; c < (uint32)a.channels.Size(); ++c) {
			const renderer::NkGLTFAnimChannel &ch = a.channels[c];
			if (ch.node < 0 || ch.node >= (int32)fbx.nodes.Size() || ch.times.Empty() ||
				ch.times.Size() != ch.values.Size())
				nodesOk = false;
			if (ch.times.Empty() || std::fabs(ch.times[0] - 0.041667f) > 1e-3f)
				timesOk = false;
			for (uint32 k = 1; k < (uint32)ch.times.Size(); ++k)
				if (ch.times[k] <= ch.times[k - 1])
					timesOk = false;
			if (ch.path == renderer::NkGLTFPath::ROTATION)
				for (uint32 k = 0; k < (uint32)ch.values.Size(); ++k) {
					const math::NkVec4f &q = ch.values[k];
					if (std::fabs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1.f) > 1e-3f)
						quatsOk = false;
				}
		}
		Check(nodesOk, "FBX : chaque canal cible un node valide (times/values paralleles)");
		Check(timesOk, "FBX : temps croissants depuis la frame 1 (0.041667 s), sans rebasage");
		Check(quatsOk, "FBX : quaternions normalises sur tous les canaux ROTATION");

		// L'ensemble des nodes animes == l'ensemble des joints du skin : chacun
		// des 19 joints porte exactement ses trois canaux T, R, S.
		bool cover = fbx.skinJoints.Size() == 19;
		for (uint32 j = 0; cover && j < (uint32)fbx.skinJoints.Size(); ++j) {
			uint32 t = 0, r = 0, s = 0;
			for (uint32 c = 0; c < (uint32)a.channels.Size(); ++c) {
				if (a.channels[c].node != fbx.skinJoints[j])
					continue;
				if (a.channels[c].path == renderer::NkGLTFPath::TRANSLATION)
					++t;
				else if (a.channels[c].path == renderer::NkGLTFPath::ROTATION)
					++r;
				else if (a.channels[c].path == renderer::NkGLTFPath::SCALE)
					++s;
			}
			cover = t == 1 && r == 1 && s == 1;
		}
		Check(cover, "FBX : chacun des 19 joints porte exactement ses canaux T, R et S");

		// Valeurs de la cle 0 du joint teste, contre l'inspecteur.
		const int32 torso = FindNodeByName(fbx, "Skeleton_torso_joint_1");
		bool t0Ok = false, r0Ok = false;
		for (uint32 c = 0; torso >= 0 && c < (uint32)a.channels.Size(); ++c) {
			const renderer::NkGLTFAnimChannel &ch = a.channels[c];
			if (ch.node != torso || ch.values.Empty())
				continue;
			const math::NkVec4f &v = ch.values[0];
			if (ch.path == renderer::NkGLTFPath::TRANSLATION)
				t0Ok = std::fabs(v.x) < 1e-4f && std::fabs(v.y - (-0.643997f)) < 1e-4f &&
					   std::fabs(v.z - (-0.020000f)) < 1e-4f;
			else if (ch.path == renderer::NkGLTFPath::ROTATION)
				r0Ok = std::fabs(v.x - 0.706668f) < 1e-3f && std::fabs(v.y - 0.025899f) < 1e-3f &&
					   std::fabs(v.z - 0.025933f) < 1e-3f && std::fabs(v.w - 0.706595f) < 1e-3f;
		}
		Check(t0Ok, "FBX : canal T de torso_joint_1, cle 0 == courbes de l'inspecteur (1e-4)");
		Check(r0Ok, "FBX : canal R de torso_joint_1, cle 0 == euler XYZ -> quat de l'inspecteur");
		logger.Infof("     FBX anim : '%s', %u canaux, duree %.6fs\n", a.name.CStr(),
					 (uint32)a.channels.Size(), a.duration);
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
	TestFBXAnim(fbx);
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
		// Etape (c) : une animation chacun. PAS de parite de duree : l'export
		// Blender a etale la timeline (10.42 s contre 2.0 s) — donnee changee
		// par l'exporteur, pas par le loader.
		Check(fbx.animations.Size() == gltf.animations.Size(),
			  "parite : autant d'animations FBX que glTF (1 chacun)");
	}

	logger.Infof("=== Resultat : %d OK, %d echec(s) ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
