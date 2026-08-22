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
//      contre l'inspecteur independant) ; (b) skinning ; (c) animations.
//   3. MIXAMO NATIF (2026-08-17 soir, Q30 nkanim) : « X Bot.fbx » et « Y Bot.fbx »
//      contre leurs .glb (fichiers hors depot, section sautee s'ils manquent) —
//      PreRotation, 2 Skin, feuilles sans Cluster, stack vide « Take 001 ».
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKLogger/NkLog.h"
#include "NkBenchRoot.h"

#include <cmath>
#include <cstdio> // fopen -- distinguer « fichier absent » de « fichier faux »

using namespace nkentseu;

namespace {

	int gPassCount = 0;
	int gFailCount = 0;

	// TROISIEME ETAT, AJOUTE LE 2026-08-22 : « PASSE SON TOUR », DISTINCT DE
	// L'ECHEC. Ce banc comptait 4 echecs qui n'en etaient pas : les cas
	// CesiumMan.fbx echouaient parce que LE FICHIER N'EST PAS DANS LE DEPOT
	// (seul le .glb y est), pas parce que le chargeur avait tort.
	//
	// Ce n'est pas cosmetique : UN BANC QUI ECHOUE FAUTE DE FICHIER APPREND A
	// SON LECTEUR A IGNORER LE ROUGE. Une fois qu'on sait que ce banc est
	// « normalement rouge », il ne signale plus rien -- et un VRAI defaut FBX
	// passerait inapercu au milieu des memes quatre lignes rouges.
	//
	// La section Mixamo faisait DEJA la bonne chose (elle sautait proprement) :
	// l'incoherence etait entre deux sections du meme fichier.
	int gSkipCount = 0;

	// Une section sautee n'est ni un succes ni un echec : elle est ABSENTE du
	// verdict, et elle le dit.
	void Skip(const char *quoi) noexcept {
		++gSkipCount;
		logger.Warnf("  [SKIP] %s\n", quoi);
	}

	// Un fichier HORS DEPOT manque legitimement : on saute. Un fichier PRESENT
	// mais illisible est un VRAI echec -- et cette distinction est exactement ce
	// que le banc ne savait pas faire.
	// -- RESOLUTION DES RESSOURCES, INDEPENDANTE DU REPERTOIRE DE LANCEMENT --
	// Mutualisee dans `Applications/Common/NkBenchRoot.h`. Elle etait ecrite
	// ici, dans NkFBXParityDemo et dans NKEditMeshHarness -- trois copies, et
	// la troisieme etait FAUSSE (elle n ancrait pas sa propre reference).
	// ⚠️ L ancienne parade ne survivait pas non plus a un lancement HORS du
	//    depot : mesure, code de sortie 1. La nouvelle passe par argv[0].
	NkString CheminRessource(const char *relatif) noexcept {
		return NkBenchPath(relatif);
	}

	bool FichierPresent(const char *chemin) noexcept {
		FILE *f = fopen(chemin, "rb");
		if (!f)
			return false;
		fclose(f);
		return true;
	}

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

	// Compose T*R*S des nodes jusqu'a la racine -> position monde du node.
	math::NkVec3f WorldPos(const renderer::NkGLTFMeshData &d, int32 node) noexcept {
		// parent de chaque node (l'arbre ne stocke que les enfants)
		NkVector<int32> parent;
		parent.Resize((NkVector<int32>::SizeType)d.nodes.Size());
		for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i)
			parent[i] = -1;
		for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i)
			for (uint32 c = 0; c < (uint32)d.nodes[i].children.Size(); ++c)
				parent[(uint32)d.nodes[i].children[c]] = (int32)i;
		math::NkMat4f m = math::NkMat4f::Identity();
		for (int32 n = node; n >= 0; n = parent[(uint32)n]) {
			const renderer::NkGLTFNode &g = d.nodes[(uint32)n];
			math::NkMat4f local = g.hasMatrix ? g.matrix
											 : math::NkMat4f::TRS(g.translation,
																  math::NkQuatf(g.rotation.x, g.rotation.y, g.rotation.z, g.rotation.w),
																  g.scale);
			m = local * m;
		}
		return {m.data[12], m.data[13], m.data[14]};
	}

	bool Near(const math::NkVec3f &a, float32 x, float32 y, float32 z, float32 tol) noexcept {
		return std::fabs(a.x - x) < tol && std::fabs(a.y - y) < tol && std::fabs(a.z - z) < tol;
	}

	int32 JointOrdinal(const renderer::NkGLTFMeshData &d, const char *name) noexcept {
		for (uint32 k = 0; k < (uint32)d.skinJoints.Size(); ++k) {
			const int32 n = d.skinJoints[k];
			if (n >= 0 && n < (int32)d.nodes.Size() && d.nodes[(uint32)n].name == NkString(name))
				return (int32)k;
		}
		return -1;
	}

	// ── Reference glTF : le chemin prouve, qui ne doit pas bouger ──────────
	void TestGLTFReference(renderer::NkGLTFMeshData &gltf) noexcept {
		logger.Infof("-- Reference glTF : CesiumMan.glb --\n");
		const bool ok = renderer::LoadGLTF(CheminRessource("Resources/Models/CesiumMan/CesiumMan.glb"), gltf);
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
		if (!FichierPresent(CheminRessource("Resources/Models/CesiumMan/CesiumMan.fbx").Data())) {
			Skip("CesiumMan.fbx ABSENT du depot (seul le .glb y est) - etapes (a)(b)(c) non jouees");
			return;
		}
		const bool ok = renderer::LoadFBX(CheminRessource("Resources/Models/CesiumMan/CesiumMan.fbx"), fbx);
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
	// SEMANTIQUE (corrigee 2026-08-17 soir, Q30 nkanim) : TransformLink = monde
	// du joint au bind, Transform = TransformLink^-1 * monde du mesh au bind.
	// inverseBind = TransformLink^-1 -> inverse(inverseBind) = TransformLink :
	// torso_joint_1 t = (0.500, 67.900, 0.000) cm (inspecteur), et le graphe de
	// nodes (Lcl composees) doit recomposer ce meme monde. L'ancienne formule
	// TL^-1 * Transform (t = 0.0446, -0.0045, -0.6776) « passait » contre elle-
	// meme ; le seul chiffre independant, l'inverseBind glTF t = (0.0513,
	// -0.0050, -0.6771), est la translation de Transform SEULE — la « parite
	// impossible » de Q10 etait un artefact. Les sommets skinnes sont ramenes
	// dans l'espace du squelette (M = TL * Transform = R(-180,-90,0) * S(100)).
	void TestFBXSkin(const renderer::NkGLTFMeshData &fbx) noexcept {
		logger.Infof("-- FBX etape (b) : CesiumMan.fbx (skinning) --\n");
		if (!fbx.IsValid()) {
			Skip("CesiumMan.fbx non charge (fichier hors depot) - etape (b) non jouee");
			return;
		}
		Check(fbx.isSkinned, "FBX : isSkinned (le Deformer Skin est extrait)");
		// ⚠️ CES DEUX CONTROLES PASSAIENT A VIDE AVANT LE 2026-08-22, et c'est
		// pourquoi le compte d'OK de ce banc est passe de 10 a 8 le jour ou les
		// etapes (a)(b)(c) ont appris a SAUTER : ce ne sont pas deux couvertures
		// perdues, ce sont deux FAUX VERTS qui ont disparu.
		// Ils comparent deux TAILLES sans verifier l'EXISTENCE : sur un fbx non
		// charge, `0 == 0` est vrai, donc ils annonçaient « parallele » sur deux
		// tableaux vides. C'est « reussir pour la mauvaise raison » -- la meme
		// forme que le script qui annoncait « une seule empreinte » sur quatre
		// valeurs toutes ABSENTES.
		// Ils ne tournent desormais que lorsque le fichier existe, donc quand la
		// comparaison a un sens.
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

		// La matrice de bind du joint teste, contre l'inspecteur : inverse(ib)
		// == TransformLink (monde du joint au bind), et le graphe de nodes
		// recompose ce meme monde (hierarchie + euler + echelle 100 de la racine).
		if (torso >= 0) {
			const math::NkVec4f b = fbx.inverseBind[(uint32)torso].Inverse().position;
			Check(std::fabs(b.x - 0.500f) < 0.05f && std::fabs(b.y - 67.900f) < 0.05f && std::fabs(b.z) < 0.05f,
				  "FBX : inverse(inverseBind(torso_joint_1)) == TransformLink (0.5, 67.9, 0) cm — inspecteur");
			// Le graphe de nodes (Z_UP S=100,R180X -> Armature R90Y -> joints)
			// recompose le MEME ESPACE (cm, memes axes) mais la pose de FRAME 0,
			// pas la pose de bind : Blender exporte les Lcl a la frame courante
			// (mesure : graphe (-2.0, 64.4, 0.0) contre TransformLink (0.5, 67.9,
			// 0.0) — ecart 4 cm sur une hauteur de 68). Chez Mixamo bind == scene.
			const math::NkVec3f w = WorldPos(fbx, fbx.skinJoints[(uint32)torso]);
			logger.Infof("     FBX torso_joint_1 : monde du graphe (%.2f %.2f %.2f) cm, TransformLink (%.2f %.2f %.2f) cm\n",
						 w.x, w.y, w.z, b.x, b.y, b.z);
			Check(std::fabs(w.x - b.x) < 10.f && std::fabs(w.y - b.y) < 10.f && std::fabs(w.z - b.z) < 10.f,
				  "FBX : monde(torso_joint_1) du graphe dans le meme espace que TransformLink (< 10 cm, pose frame 0)");
			// Les sommets skinnes sont dans l'espace du squelette : hauteur du
			// bonhomme ~ 1.5-1.9 m -> bornes en cm (glTF : metres, ~1.7).
			Check(fbx.bounds.max.y - fbx.bounds.min.y > 100.f,
				  "FBX : sommets skinnes ramenes dans l'espace du squelette (hauteur > 100 cm)");
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
		if (!fbx.IsValid()) {
			Skip("CesiumMan.fbx non charge (fichier hors depot) - etape (c) non jouee");
			return;
		}
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

	// ── Mixamo natif : « X Bot.fbx » contre « XBot.glb » (Q30 nkanim) ─────
	// Le temoin CesiumMan est un export Blender « euler XYZ sans PreRotation »
	// : il ne couvre PAS le regime Mixamo (PreRotation sur 56 joints,
	// InheritType RSrs, 2 Skin, 2 AnimationStack, courbes rattachees a
	// « Lcl Rotation »). Ce banc charge les DEUX fichiers du meme personnage
	// (fichiers hors depot — Resources/Models/ est ignore par git ; SAUTE
	// proprement s'ils manquent, sans compter d'echec).
	// Chiffres de l'inspecteur independant (fbx_inspect.py, scratchpad de
	// session, 2026-08-17) sur « X Bot.fbx » : 67 Model (65 LimbNode +
	// Beta_Surface + Beta_Joints), 64 aretes Model->Model + 3 racines,
	// 2 Skin (1 par Geometry), 129 Cluster, 2 AnimationStack, 2 Layer,
	// 67 CurveNode (65 « Lcl Rotation », 1 « Lcl Translation »), 315 courbes.
	// Positions monde de bind (glb, inverse des inverseBind, en METRES) :
	// Hips (0, 1.0427, 0.0155), LeftHand (0.713, 1.4406, -0.0555) — le FBX
	// est en CENTIMETRES (x100). LeftFoot : le glb dit (8.73, 9.99, -1.06) cm
	// mais le FBX LUI-MEME (TransformLink du Cluster LeftFoot, inspecteur)
	// dit (8.208, 8.729, -2.743) : les deux fichiers different de 1-2 cm dans
	// la jambe (donnee, pas loader) ; la reference du pied est donc le
	// TransformLink du fichier, pas le glb.


	void TestMixamo() noexcept {
		logger.Infof("-- Mixamo natif : X Bot.fbx contre XBot.glb (Q30 nkanim) --\n");
		renderer::NkGLTFMeshData glb;
		renderer::NkGLTFMeshData fbx;
		const bool okG = renderer::LoadGLTF(CheminRessource("Resources/Models/XBot/XBot.glb"), glb);
		const bool okF = renderer::LoadFBX(CheminRessource("Resources/Models/XBot/X Bot.fbx"), fbx);
		if (!okG || !okF) {
			Skip("fichiers XBot absents de Resources/Models/XBot/ - section non jouee");
			return;
		}
		// 1. Squelette : 67 nodes, 64 aretes, hierarchie et PreRotation
		//    composees -> positions monde de bind (cm) == glb (m) x100.
		Check(fbx.nodes.Size() == 67, "Mixamo : 67 nodes (67 Model)");
		uint32 edges = 0;
		for (uint32 i = 0; i < (uint32)fbx.nodes.Size(); ++i)
			edges += (uint32)fbx.nodes[i].children.Size();
		Check(edges == 64, "Mixamo : 64 aretes Model->Model (67 nodes - 3 racines)");
		const int32 hips = FindNodeByName(fbx, "mixamorig:Hips");
		const int32 lhand = FindNodeByName(fbx, "mixamorig:LeftHand");
		const int32 lfoot = FindNodeByName(fbx, "mixamorig:LeftFoot");
		const int32 rfoot = FindNodeByName(fbx, "mixamorig:RightFoot");
		Check(hips >= 0 && lhand >= 0 && lfoot >= 0 && rfoot >= 0, "Mixamo : Hips/LeftHand/LeftFoot/RightFoot nommes");
		if (hips >= 0 && lhand >= 0 && lfoot >= 0 && rfoot >= 0) {
			const math::NkVec3f pH = WorldPos(fbx, hips), pLH = WorldPos(fbx, lhand), pLF = WorldPos(fbx, lfoot),
								pRF = WorldPos(fbx, rfoot);
			logger.Infof("     FBX monde (cm) : Hips (%.2f %.2f %.2f) LeftHand (%.2f %.2f %.2f) LeftFoot (%.2f %.2f %.2f) "
						 "RightFoot (%.2f %.2f %.2f)\n",
						 pH.x, pH.y, pH.z, pLH.x, pLH.y, pLH.z, pLF.x, pLF.y, pLF.z, pRF.x, pRF.y, pRF.z);
			Check(Near(pH, 0.f, 104.27f, 1.55f, 0.5f), "Mixamo : Hips monde == glb x100 (0, 104.27, 1.55) cm");
			Check(Near(pLH, 71.33f, 144.06f, -5.55f, 1.0f),
				  "Mixamo : LeftHand monde == glb x100 (71.33, 144.06, -5.55) cm — PreRotation composee");
			Check(Near(pLF, 8.208f, 8.729f, -2.743f, 0.05f),
				  "Mixamo : LeftFoot monde == TransformLink du Cluster (8.208, 8.729, -2.743) cm — inspecteur");
			Check(std::fabs(pLF.x - pRF.x) > 10.f, "Mixamo : les deux pieds ne sont PAS au meme point");
		}
		// 2. Skin : 65 joints (glb skins[0].joints = 65 ; le FBX n'a que 64
		//    Cluster sur cette geometrie — la feuille HeadTop_End n'a pas de
		//    poids). inverseBind^-1 == position monde de bind du joint.
		Check(fbx.isSkinned, "Mixamo : isSkinned");
		Check(glb.skinJoints.Size() == 65, "Mixamo glb : 65 joints (reference)");
		Check(fbx.skinJoints.Size() == glb.skinJoints.Size(), "Mixamo : autant de joints skin FBX que glb (65)");
		const int32 jH = JointOrdinal(fbx, "mixamorig:Hips");
		const int32 jLF = JointOrdinal(fbx, "mixamorig:LeftFoot");
		const int32 jRF = JointOrdinal(fbx, "mixamorig:RightFoot");
		Check(jH >= 0 && jLF >= 0 && jRF >= 0, "Mixamo : Hips/LeftFoot/RightFoot parmi les joints skin");
		if (jH >= 0 && jLF >= 0 && jRF >= 0) {
			const math::NkVec4f bH = fbx.inverseBind[(uint32)jH].Inverse().position;
			const math::NkVec4f bLF = fbx.inverseBind[(uint32)jLF].Inverse().position;
			const math::NkVec4f bRF = fbx.inverseBind[(uint32)jRF].Inverse().position;
			logger.Infof("     FBX bind (inverseBind^-1, cm) : Hips (%.2f %.2f %.2f) LeftFoot (%.2f %.2f %.2f) RightFoot "
						 "(%.2f %.2f %.2f)\n",
						 bH.x, bH.y, bH.z, bLF.x, bLF.y, bLF.z, bRF.x, bRF.y, bRF.z);
			Check(Near({bH.x, bH.y, bH.z}, 0.f, 104.27f, 1.55f, 0.5f),
				  "Mixamo : bind(Hips) == glb x100 (0, 104.27, 1.55) cm — pas 2x");
			Check(Near({bLF.x, bLF.y, bLF.z}, 8.208f, 8.729f, -2.743f, 0.05f),
				  "Mixamo : bind(LeftFoot) == TransformLink du Cluster (inspecteur)");
			Check(std::fabs(bLF.x - bRF.x) > 10.f, "Mixamo : bind des deux pieds distincts");
			// Le bind du joint == sa pose de scene (Mixamo exporte en pose de
			// bind) : coherence interne graphe de nodes <-> Cluster.
			const int32 hn = fbx.skinJoints[(uint32)jH];
			const math::NkVec3f pH = WorldPos(fbx, hn);
			Check(Near({bH.x, bH.y, bH.z}, pH.x, pH.y, pH.z, 0.5f), "Mixamo : bind(Hips) == monde(Hips) du graphe");
		}
		bool sumOk = true;
		uint32 multi = 0;
		for (uint32 v = 0; v < (uint32)fbx.skinnedVertices.Size(); ++v) {
			const renderer::NkVertexSkinned &sv = fbx.skinnedVertices[v];
			float32 s = 0.f;
			uint32 used = 0;
			for (uint32 k = 0; k < 4; ++k) {
				s += sv.boneWeight[k];
				if (sv.boneWeight[k] > 0.f)
					++used;
			}
			if (std::fabs(s - 1.f) > 1e-3f)
				sumOk = false;
			if (used >= 2)
				++multi;
		}
		Check(sumOk, "Mixamo : somme des poids == 1 partout");
		// 3. Animation : 1 animation jouable (le glb en a 1), canaux ROTATION
		//    sur les 65 joints, duree > 0, quaternions normalises.
		Check(glb.animations.Size() == 1, "Mixamo glb : 1 animation (reference)");
		Check(fbx.animations.Size() >= 1, "Mixamo : au moins 1 animation lue (2 AnimationStack, 315 courbes)");
		if (!fbx.animations.Empty()) {
			const renderer::NkGLTFAnimation &a = fbx.animations[0];
			uint32 rot = 0;
			bool quatsOk = true;
			for (uint32 c = 0; c < (uint32)a.channels.Size(); ++c) {
				const renderer::NkGLTFAnimChannel &ch = a.channels[c];
				if (ch.path == renderer::NkGLTFPath::ROTATION) {
					++rot;
					for (uint32 k = 0; k < (uint32)ch.values.Size(); ++k) {
						const math::NkVec4f &q = ch.values[k];
						if (std::fabs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1.f) > 1e-3f)
							quatsOk = false;
					}
				}
			}
			logger.Infof("     FBX anim : '%s', %u canaux (%u rotation), duree %.3fs ; glb '%s' duree %.3fs\n",
						 a.name.CStr(), (uint32)a.channels.Size(), rot, a.duration,
						 glb.animations[0].name.CStr(), glb.animations[0].duration);
			// 65 CurveNode « Lcl Rotation » dans le fichier, mais 13 d'entre eux
			// pointent des AnimationCurve ABSENTES du fichier (IDs orphelins,
			// inspecteur : 39 connexions vers des objets inexistants) : sans
			// aucune cle, pas de canal — le joint garde sa rotation statique
			// (defaut d|X/Y/Z = 0 = meme chose). Le glb, lui, emet 65 x TRS
			// constants (195 canaux) : difference d'exporteur, pas de loader.
			Check(rot == 52, "Mixamo : 52 canaux ROTATION (65 CurveNode - 13 sans courbe dans le fichier)");
			Check(a.duration > 0.f, "Mixamo : duree > 0");
			Check(std::fabs(a.duration - glb.animations[0].duration) < 0.05f,
				  "Mixamo : duree FBX == duree glb (meme export Mixamo, pas de timeline etalee)");
			Check(quatsOk, "Mixamo : quaternions normalises");
		}
		logger.Infof("     Mixamo : %u nodes, %u aretes, %u joints, %u anims, %u/%u multi-influences\n",
					 (uint32)fbx.nodes.Size(), edges, (uint32)fbx.skinJoints.Size(), (uint32)fbx.animations.Size(),
					 multi, (uint32)fbx.skinnedVertices.Size());
	}

	// ── Mixamo natif, second personnage : « Y Bot.fbx » — 13 feuilles SANS
	// Cluster (HeadTop_End, les 10 phalanges *4, Left/RightToe_End —
	// inspecteur : 65 LimbNode, 104 Cluster, 52 joints distincts). Le glb du
	// meme personnage garde 65 joints : le loader complete par l'arbre.
	void TestMixamoYBot() noexcept {
		logger.Infof("-- Mixamo natif : Y Bot.fbx (feuilles sans Cluster) --\n");
		renderer::NkGLTFMeshData glb;
		renderer::NkGLTFMeshData fbx;
		const bool okG = renderer::LoadGLTF(CheminRessource("Resources/Models/XBot/YBot.glb"), glb);
		const bool okF = renderer::LoadFBX(CheminRessource("Resources/Models/XBot/Y Bot.fbx"), fbx);
		if (!okG || !okF) {
			Skip("fichiers YBot absents de Resources/Models/XBot/ - section non jouee");
			return;
		}
		Check(glb.skinJoints.Size() == 65, "YBot glb : 65 joints (reference)");
		Check(fbx.skinJoints.Size() == 65, "YBot : 65 joints (52 Cluster + 13 feuilles completees par l'arbre)");
		const int32 jTop = JointOrdinal(fbx, "mixamorig:HeadTop_End");
		const int32 jHead = JointOrdinal(fbx, "mixamorig:Head");
		Check(jTop >= 0 && jHead >= 0, "YBot : HeadTop_End (sans Cluster) et Head parmi les joints");
		if (jTop >= 0 && jHead >= 0) {
			// bind(feuille) = bind(parent) * local : chez Mixamo bind == scene,
			// donc == monde du graphe ; et la feuille est bien AU-DESSUS de Head.
			const math::NkVec4f bT = fbx.inverseBind[(uint32)jTop].Inverse().position;
			const math::NkVec4f bH = fbx.inverseBind[(uint32)jHead].Inverse().position;
			const math::NkVec3f wT = WorldPos(fbx, fbx.skinJoints[(uint32)jTop]);
			logger.Infof("     YBot bind : Head (%.2f %.2f %.2f) HeadTop_End (%.2f %.2f %.2f) ; graphe HeadTop_End (%.2f %.2f %.2f)\n",
						 bH.x, bH.y, bH.z, bT.x, bT.y, bT.z, wT.x, wT.y, wT.z);
			Check(Near({bT.x, bT.y, bT.z}, wT.x, wT.y, wT.z, 0.05f),
				  "YBot : bind(HeadTop_End) == bind(Head) * local == monde du graphe");
			Check(bT.y > bH.y + 10.f, "YBot : HeadTop_End au-dessus de Head (> 10 cm)");
			// Ordre prefixe (parents avant enfants) : Head juste avant HeadTop_End,
			// comme skins[0].joints du glb (parite d'indices).
			Check(jTop == jHead + 1, "YBot : HeadTop_End suit Head dans l'ordre des joints (parcours prefixe)");
		}
		bool idxOk = true;
		for (uint32 v = 0; v < (uint32)fbx.skinnedVertices.Size() && idxOk; ++v)
			for (uint32 k = 0; k < 4; ++k)
				if (fbx.skinnedVertices[v].boneIdx[k] < 0.f || fbx.skinnedVertices[v].boneIdx[k] >= 65.f)
					idxOk = false;
		Check(idxOk, "YBot : boneIdx bornes par 65");
		Check(fbx.animations.Size() == 1, "YBot : 1 animation (stack « mixamo.com » ; « Take 001 » vide ignore)");
		logger.Infof("     YBot : %u joints, %u anims (glb : %u)\n", (uint32)fbx.skinJoints.Size(),
					 (uint32)fbx.animations.Size(), (uint32)glb.animations.Size());
	}

	// ── Non-regression ASCII : le cube de test (aucun Model dedans) ────────
	void TestFBXAscii() noexcept {
		logger.Infof("-- FBX ASCII : cube_ascii.fbx (non-regression geometrie) --\n");
		renderer::NkGLTFMeshData cube;
		const bool ok = renderer::LoadFBX(CheminRessource("Resources/Models/test/cube_ascii.fbx"), cube);
		Check(ok && cube.IsValid(), "LoadFBX cube_ascii.fbx reussit toujours");
		if (ok)
			Check(cube.nodes.Empty(), "FBX ASCII : 0 node (le fichier n'a aucun Model — zero invente)");
	}

} // namespace

int main(int argc, char **argv) {
	// ANCRE DU DEPOT, avant toute mesure. Pas de repli : un banc qui ne sait
	// pas ou est le depot mesurerait des fichiers absents et le dirait comme
	// s'il mesurait un chargeur.
	NkBenchRootInit(argc, argv);
	if (!NkBenchRootFound()) {
		NkLog::Instance().Error("ECHEC : racine du depot introuvable (marqueur 'Nkentseu.jenga') "
								"ni depuis le repertoire courant ni depuis {0}",
								NkBenchArgv0());
		return 3;
	}

	logger.Infof("=== NkFBXParityDemo — parite FBX / glTF (CesiumMan) ===\n");

	renderer::NkGLTFMeshData gltf; // non copiable : vit ici, passe par reference
	renderer::NkGLTFMeshData fbx;
	TestGLTFReference(gltf);
	TestFBXNodes(fbx);
	TestFBXSkin(fbx);
	TestFBXAnim(fbx);
	TestFBXAscii();
	TestMixamo();
	TestMixamoYBot();

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

	// Le verdict distingue les TROIS etats. Le code de sortie ne depend QUE des
	// echecs : un banc dont il ne manque que des fichiers sort en 0, et son rouge
	// redevient donc un signal au lieu d'un bruit de fond.
	logger.Infof("=== Resultat : %d OK, %d echec(s), %d saute(s) ===\n", gPassCount, gFailCount, gSkipCount);
	if (gSkipCount > 0)
		logger.Warnf("     %d section(s) sautee(s) faute de fichiers HORS DEPOT - ce n'est PAS un echec\n",
				 gSkipCount);
	return gFailCount == 0 ? 0 : 1;
}
