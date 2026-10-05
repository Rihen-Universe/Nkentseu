// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_rig3d.cpp — LE RIG 3D, LA PEAU, LES FORMES (02/10/2026).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (r1)  L'ARMATURE : le repere d'un os a Y sur tete -> queue et des axes
//         orthonormes ; un squelette IMPORTE (joints tournes au hasard) rend
//         EXACTEMENT ses joints (JointMatrix == monde, 1e-4) ; extruder,
//         subdiviser, supprimer, deplacer une tete connectee (la queue du
//         parent suit), refuser un cycle, renommer en unique ; la SYMETRIE X
//         nomme .L/.R, miroite les positions, le roulis et le parent ;
//         CONTRE-EPREUVE : sans le decalage de joint, le meme temoin voit ECHEC
//   (r2)  LE DOCUMENT ET SON ANNULATION : ajouter / supprimer un os (ses poids
//         vont au parent), Ctrl+Z rend l'etat d'avant AU BIT PRES, Ctrl+Y celui
//         d'apres ; un curseur glisse fusionne en UNE entree
//   (r3)  LE RIG AUTOMATIQUE sur un mannequin SANS squelette (pose en T, en A,
//         quadrupede) : reperes trouves, symetriques (1 % de la hauteur), aine a
//         sa hauteur ; l'armature construite a TOUS ses os dans le volume
//         (milieu de chaque os dedans) et sa symetrie ; les controles IK des
//         bras et des jambes ; CONTRE-EPREUVE : l'armature decalee de 0,5 hors
//         du corps -> le temoin « dans le volume » voit ECHEC
//   (r4)  LES POIDS AUTOMATIQUES (chaleur ET voxels geodesiques) : chaque
//         sommet pondere, somme = 1, au plus 4 os ; le bout de la main gauche
//         appartient a « hand.L » ; CONTRE-EPREUVE : une somme faussee est vue
//         par la verification
//   (r5)  LA PEINTURE : un trait ajoute, la somme reste 1 (normalisation
//         automatique), la symetrie peint le miroir ; Ctrl+Z rend les poids au
//         bit pres ; CONTRE-EPREUVE : sans normalisation, la verification voit
//         des sommes fausses
//   (r6)  LES FORMES : a 0,5 une forme relative a la base rend la MOYENNE
//         EXACTE ; relative a une autre forme, elle n'ajoute que sa difference ;
//         le miroir d'une forme ; un PILOTE (rotation d'os 45 deg sur 0..90)
//         la met a 0,5 ; CONTRE-EPREUVE : relative a la mauvaise forme, la
//         moyenne n'est plus la
//   (r7)  UNE PISTE DE FORME dans un clip : jouee a t = 0,5 elle vaut 0,5 ;
//         elle fait l'aller-retour .nkanim (section MRPH) ; CONTRE-EPREUVE : un
//         nom qui ne correspond a aucune forme ne joue rien
//   (r8)  L'IK A DEUX OS en 3D : la cible est atteinte (1e-4), le coude dans le
//         plan du pole et de son cote ; hors de portee la chaine se tend ;
//         CONTRE-EPREUVE : une longueur fausse rate la cible
//   (r9)  LES FORMES DE VISAGE de depart : creees sur la tete seulement
//         (machoire vers le bas, clin d'oeil d'un seul cote)
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKAnima/Rig/NkRigMath.h"
#include "NKFileSystem/NkFile.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim::rigm;
using math::NkMat4f;
using math::NkVec3f;

namespace {
	bool Pres(float32 a, float32 b, float32 tol = 1e-4f) {
		return std::fabs(a - b) <= tol;
	}
	bool PresV(const NkVec3f &a, const NkVec3f &b, float32 tol = 1e-4f) {
		return Dist(a, b) <= tol;
	}
	bool MemeMatrice(const NkMat4f &a, const NkMat4f &b, float32 tol = 1e-4f) {
		for (int32 i = 0; i < 16; ++i) {
			if (std::fabs(a.data[i] - b.data[i]) > tol) {
				return false;
			}
		}
		return true;
	}
	/// Une rotation de `angle` radians autour d'un axe unitaire, en matrice (avec origine).
	NkMat4f Rot(const NkVec3f &axe, float32 angle, const NkVec3f &o) {
		const NkVec3f k = Norm(axe);
		return Repere(Tourner(V(1, 0, 0), k, angle), Tourner(V(0, 1, 0), k, angle), Tourner(V(0, 0, 1), k, angle), o);
	}

	/// LE mannequin du banc, fabrique une fois (le calcul prend un instant).
	const anim::NkSkinMesh &Mannequin(anim::NkMannequinKind k) {
		static anim::NkSkinMesh m[3];
		static bool fait[3] = {false, false, false};
		const uint32 i = (uint32)k;
		if (!fait[i]) {
			anim::NkMakeMannequin(k, m[i], 80);
			m[i].BuildMirror();
			fait[i] = true;
		}
		return m[i];
	}

	/// Combien d'os ont leur MILIEU dans le volume (et combien en tout).
	uint32 OsDedans(const anim::NkSkinMesh &m, const anim::NkArmature &a, const NkVec3f &decalage, bool dire = false) {
		uint32 n = 0;
		for (uint32 b = 0; b < a.Count(); ++b) {
			const NkVec3f mil = Add(Lerp(a.bones[b].head, a.bones[b].tail, 0.5f), decalage);
			const bool d = m.Contains(mil);
			n += d ? 1u : 0u;
			if (!d && dire) {
				std::printf("        hors du volume : %s (milieu %.3f %.3f %.3f)\n", a.bones[b].name.CStr(), (double)mil.x, (double)mil.y, (double)mil.z);
			}
		}
		return n;
	}

	/// Toutes les influences, en clair, pour comparer deux etats au bit pres.
	bool MemesPoids(const anim::NkSkinWeights &a, const anim::NkSkinWeights &b) {
		if (a.VertexCount() != b.VertexCount() || a.data.Size() != b.data.Size()) {
			return false;
		}
		return std::memcmp(a.data.Data(), b.data.Data(), a.data.Size() * sizeof(anim::NkSkinInfluence)) == 0;
	}
} // namespace

// (r1)
TEST_CASE(NKAnima, RIG3D_r1_Armature) {
	anim::NkArmature a;
	const int32 b0 = a.Add("bras", V(0.1f, 1.f, 0.f), V(0.5f, 1.2f, 0.1f));
	a.SetRoll((uint32)b0, 0.7f);
	const NkMat4f m = a.BoneMatrix((uint32)b0);
	const NkVec3f x = AxeX(m), y = AxeY(m), z = AxeZ(m);
	ASSERT_TRUE(PresV(y, Norm(Sub(a.bones[0].tail, a.bones[0].head))));
	ASSERT_TRUE(Pres(Len(x), 1.f) && Pres(Len(z), 1.f) && Pres(Dot(x, y), 0.f) && Pres(Dot(y, z), 0.f) && Pres(Dot(x, z), 0.f));
	ASSERT_TRUE(PresV(Cross(x, y), z)); // repere direct
	// Un os vers le BAS (le cas special de Blender) reste un repere direct.
	const int32 bas = a.Add("jambe", V(0.f, 1.f, 0.f), V(0.f, 0.f, 0.f));
	const NkMat4f mb = a.BoneMatrix((uint32)bas);
	ASSERT_TRUE(PresV(Cross(AxeX(mb), AxeY(mb)), AxeZ(mb)) && Pres(AxeY(mb).y, -1.f));

	// IMPORT : des joints tournes au hasard rendent EXACTEMENT leurs matrices.
	NkMat4f monde[5];
	const int32 parents[5] = {-1, 0, 1, 1, 3};
	monde[0] = Rot(V(0.3f, 1.f, 0.2f), 0.4f, V(0.f, 1.f, 0.f));
	monde[1] = Rot(V(1.f, 0.f, 0.5f), 1.1f, V(0.f, 1.3f, 0.05f));
	monde[2] = Rot(V(0.f, 0.f, 1.f), -0.6f, V(0.f, 1.6f, 0.f));
	monde[3] = Rot(V(0.7f, 0.2f, 0.1f), 2.2f, V(0.2f, 1.5f, 0.f));
	monde[4] = Rot(V(0.1f, 0.9f, 0.4f), 0.9f, V(0.45f, 1.5f, 0.f));
	NkString noms[5] = {NkString("hips"), NkString("spine"), NkString("head"), NkString("upperarm.L"), NkString("hand.L")};
	const anim::NkArmature imp = anim::NkArmature::FromJoints(monde, parents, noms, 5);
	ASSERT_TRUE(imp.Count() == 5);
	bool exact = true, sansDecalage = true;
	for (uint32 j = 0; j < 5; ++j) {
		exact = exact && MemeMatrice(imp.JointMatrix(j), monde[j]);
		sansDecalage = sansDecalage && MemeMatrice(imp.BoneMatrix(j), monde[j]);
	}
	std::printf("  [r1] import : JointMatrix == monde %s ; contre-epreuve sans decalage : %s\n", exact ? "OUI" : "NON",
				sansDecalage ? "egal (ECHEC de la contre-epreuve)" : "different (ECHEC vu)");
	ASSERT_TRUE(exact);
	ASSERT_FALSE(sansDecalage);
	ASSERT_TRUE(imp.bones[2].connected); // spine -> head : seul enfant, tete sur la queue
	ASSERT_TRUE(PresV(imp.bones[1].tail, imp.bones[2].head));

	// EDITION : extruder, subdiviser, deplacer une tete connectee, cycle, noms.
	anim::NkArmature e;
	const int32 r = e.Add("Os", V(0, 0, 0), V(0, 1, 0));
	const int32 c = e.Extrude((uint32)r, V(0, 2, 0));
	ASSERT_TRUE(c == 1 && e.bones[1].connected && e.bones[1].name == NkString("Os.001"));
	e.SetHead((uint32)c, V(0.f, 1.5f, 0.f));
	ASSERT_TRUE(PresV(e.bones[0].tail, V(0.f, 1.5f, 0.f))); // la queue du parent suit
	ASSERT_TRUE(e.Subdivide(0, 2) && e.Count() == 4);
	ASSERT_TRUE(Pres(e.bones[0].tail.y, 0.5f) && e.bones[1].parent == 3); // l'enfant passe au dernier segment
	ASSERT_FALSE(e.SetParent(0, 1, false));								 // un cycle est refuse
	ASSERT_TRUE(e.Rename(1, "Pied") == NkString("Pied") && e.Rename(2, "Pied") == NkString("Pied.001"));
	NkVector<int32> remap;
	ASSERT_TRUE(e.Remove(3, &remap) && e.Count() == 3 && remap[3] == -1 && e.bones[1].parent == 2);

	// SYMETRIE X : noms, positions, roulis, parent.
	anim::NkArmature s;
	const int32 tronc = s.Add("chest", V(0, 1, 0), V(0, 1.4f, 0));
	const int32 ep = s.Add("shoulder.L", V(0.05f, 1.35f, 0.f), V(0.2f, 1.38f, 0.02f), tronc, false);
	const int32 br = s.Add("upperarm.L", V(0.2f, 1.38f, 0.02f), V(0.45f, 1.3f, 0.f), ep, true);
	s.SetRoll((uint32)br, 0.3f);
	const int32 mep = s.Symmetrize((uint32)ep);
	const int32 mbr = s.Symmetrize((uint32)br);
	ASSERT_TRUE(mep >= 0 && mbr >= 0 && s.bones[(uint32)mbr].name == NkString("upperarm.R"));
	ASSERT_TRUE(PresV(s.bones[(uint32)mbr].head, MiroirX(s.bones[(uint32)br].head)) && Pres(s.bones[(uint32)mbr].roll, -0.3f));
	ASSERT_TRUE(s.bones[(uint32)mbr].parent == mep && s.bones[(uint32)mbr].connected);
	// Le repere du miroir EST le miroir du repere (S M S).
	ASSERT_TRUE(MemeMatrice(s.JointMatrix((uint32)mbr), MiroirMatriceX(s.JointMatrix((uint32)br))));
	ASSERT_TRUE(anim::NkArmature::MirrorName("LeftHand") == NkString("RightHand"));
	ASSERT_TRUE(anim::NkArmature::MirrorName("main_gauche") == NkString("main_droite"));
	ASSERT_TRUE(anim::NkArmature::MirrorName("hips").Empty());
	// (05/10) LES INFIXES : CesiumMan nomme ses os « Skeleton_arm_joint_L__2_ » et
	// Blender double « hand.L.001 ». Sans les infixes, ces os n'avaient AUCUN cote :
	// pas de miroir, pas de couleur .L / .R, la symetrie des poids les ignorait.
	ASSERT_TRUE(anim::NkArmature::MirrorName("Skeleton_arm_joint_L__2_") == NkString("Skeleton_arm_joint_R__2_"));
	ASSERT_TRUE(anim::NkArmature::SideOfName("Skeleton_arm_joint_L__2_") == anim::NkBoneSide::NK_GAUCHE);
	ASSERT_TRUE(anim::NkArmature::SideOfName("Skeleton_leg_joint_R__3_") == anim::NkBoneSide::NK_DROITE);
	ASSERT_TRUE(anim::NkArmature::MirrorName("hand.L.001") == NkString("hand.R.001"));
	// Contre-epreuve : un nom SANS cote reste sans cote (l'infixe ne se devine pas).
	ASSERT_TRUE(anim::NkArmature::MirrorName("Skeleton_neck_joint_1").Empty());
	ASSERT_TRUE(anim::NkArmature::SideOfName("Skeleton_torso_joint_2") == anim::NkBoneSide::NK_CENTRE);
	ASSERT_TRUE(s.Symmetrize((uint32)tronc) < 0); // un os du milieu n'a pas de miroir
}

// (r2)
TEST_CASE(NKAnima, RIG3D_r2_DocumentAnnulation) {
	anim::NkRigDocument d;
	const anim::NkSkinMesh &mq = Mannequin(anim::NkMannequinKind::NK_HUMANOIDE_T);
	d.SetMesh(mq.positions.Data(), mq.VertexCount(), mq.indices.Data(), (uint32)mq.indices.Size());
	ASSERT_TRUE(d.mesh.VertexCount() > 1000 && !d.CanUndo());
	const int32 r = d.AddBone("racine", V(0, 0.9f, 0), V(0, 1.2f, 0), -1, false);
	const int32 b = d.AddBone("cuisse.L", V(0.1f, 0.9f, 0), V(0.1f, 0.5f, 0), r, false);
	ASSERT_TRUE(d.armature.Count() == 3); // la symetrie a cree cuisse.R
	ASSERT_TRUE(d.armature.Find("cuisse.R") >= 0);
	// Des poids a la main : la moitie des sommets sur la cuisse gauche.
	for (uint32 v = 0; v < d.weights.VertexCount(); ++v) {
		d.weights.Set(v, (v % 2) ? b : r, 1.f);
	}
	d.Touch(anim::NK_RIG_POIDS);
	const anim::NkSkinWeights avant = d.weights;
	ASSERT_TRUE(d.DeleteBone((uint32)b));
	ASSERT_TRUE(d.armature.Count() == 1); // le miroir part avec (symetrie)
	// Les poids de la cuisse sont passes au parent : tout est sur la racine.
	bool toutRacine = true;
	for (uint32 v = 0; v < d.weights.VertexCount(); ++v) {
		toutRacine = toutRacine && Pres(d.weights.Get(v, 0), 1.f);
	}
	ASSERT_TRUE(toutRacine);
	ASSERT_TRUE(d.Undo());
	ASSERT_TRUE(d.armature.Count() == 3 && MemesPoids(d.weights, avant));
	ASSERT_TRUE(d.Redo());
	ASSERT_TRUE(d.armature.Count() == 1);
	ASSERT_TRUE(d.Undo());
	// Un curseur glisse : UNE entree.
	const int32 k = d.AddShape("sourire");
	const uint32 prof = d.UndoDepth();
	for (int32 i = 1; i <= 10; ++i) {
		d.SetShapeValue((uint32)k, 0.1f * (float32)i);
	}
	std::printf("  [r2] profondeur avant le curseur %u, apres dix valeurs %u\n", prof, d.UndoDepth());
	ASSERT_TRUE(d.UndoDepth() == prof + 1u);
	ASSERT_TRUE(d.Undo() && Pres(d.shapes.keys[(uint32)k].value, 0.f));
}

// (r3)
TEST_CASE(NKAnima, RIG3D_r3_RigAutomatique) {
	const anim::NkMannequinKind kinds[2] = {anim::NkMannequinKind::NK_HUMANOIDE_T, anim::NkMannequinKind::NK_HUMANOIDE_A};
	for (uint32 q = 0; q < 2; ++q) {
		const anim::NkSkinMesh &m = Mannequin(kinds[q]);
		const float32 H = m.bmax.y - m.bmin.y;
		ASSERT_TRUE(m.Contains(V(0.f, 1.2f, 0.f)) && !m.Contains(V(0.f, 1.2f, 1.f)));
		NkVector<anim::NkRigLandmark> l;
		anim::NkAutoRigReport rp;
		anim::NkAutoRigOptions opt;
		ASSERT_TRUE(anim::NkRigDetectLandmarks(m, anim::NkRigTemplate::NK_HUMANOIDE, opt, l, &rp));
		std::printf("  [r3] %s : %s\n", q == 0 ? "pose en T" : "pose en A", rp.message.CStr());
		// L'aine du mannequin est a ~0,8 / 1,77.
		ASSERT_TRUE(rp.aineY > 0.38f * H && rp.aineY < 0.56f * H);
		// La symetrie des reperes : 1 % de la hauteur.
		float32 ecart = 0.f;
		for (uint32 i = 0; i < (uint32)l.Size(); ++i) {
			const NkString mi = anim::NkArmature::MirrorName(l[i].id.CStr());
			const int32 j = mi.Empty() ? -1 : anim::NkRigFindLandmark(l, mi.CStr());
			if (j >= 0) {
				const float32 e = Dist(l[i].position, MiroirX(l[(uint32)j].position));
				ecart = e > ecart ? e : ecart;
			} else {
				ecart = std::fabs(l[i].position.x) > ecart ? std::fabs(l[i].position.x) : ecart;
			}
		}
		std::printf("  [r3] ecart de symetrie des reperes : %.4f (%.2f %% de la hauteur)\n", (double)ecart, (double)(ecart / H * 100.f));
		ASSERT_TRUE(ecart < 0.01f * H);
		anim::NkArmature a;
		ASSERT_TRUE(anim::NkRigBuildArmature(anim::NkRigTemplate::NK_HUMANOIDE, l, opt, a));
		ASSERT_TRUE(a.Count() == 21);
		const uint32 dedans = OsDedans(m, a, V(0, 0, 0), true);
		const uint32 dehors = OsDedans(m, a, V(0.f, 0.f, 0.5f));
		std::printf("  [r3] os dans le volume : %u / %u ; contre-epreuve (decale de 0,5 en Z) : %u / %u\n", dedans, a.Count(), dehors, a.Count());
		ASSERT_TRUE(dedans == a.Count());
		ASSERT_TRUE(dehors < a.Count() / 4u); // ECHEC vu : la contre-epreuve sait rougir
		// La symetrie de l'armature.
		for (uint32 b = 0; b < a.Count(); ++b) {
			const NkString mi = anim::NkArmature::MirrorName(a.bones[b].name.CStr());
			if (!mi.Empty()) {
				const int32 j = a.Find(mi.CStr());
				ASSERT_TRUE(j >= 0);
				ASSERT_TRUE(PresV(a.bones[b].head, MiroirX(a.bones[(uint32)j].head), 0.012f * H));
			}
		}
		NkVector<anim::NkRigControl> ctl;
		anim::NkRigGenerateControls(anim::NkRigTemplate::NK_HUMANOIDE, a, ctl);
		uint32 ik = 0;
		for (uint32 c = 0; c < (uint32)ctl.Size(); ++c) {
			ik += ctl[c].kind == anim::NkRigControlKind::NK_IK_DEUX_OS && ctl[c].chain.Size() == 3 ? 1u : 0u;
		}
		ASSERT_TRUE(ik == 4u && ctl.Size() >= 7u);
	}
	// Le QUADRUPEDE.
	{
		const anim::NkSkinMesh &m = Mannequin(anim::NkMannequinKind::NK_QUADRUPEDE);
		NkVector<anim::NkRigLandmark> l;
		anim::NkAutoRigReport rp;
		anim::NkAutoRigOptions opt;
		ASSERT_TRUE(anim::NkRigDetectLandmarks(m, anim::NkRigTemplate::NK_QUADRUPEDE, opt, l, &rp));
		anim::NkArmature a;
		ASSERT_TRUE(anim::NkRigBuildArmature(anim::NkRigTemplate::NK_QUADRUPEDE, l, opt, a));
		const uint32 dedans = OsDedans(m, a, V(0, 0, 0), true);
		std::printf("  [r3] quadrupede : %s ; %u / %u os dans le volume\n", rp.message.CStr(), dedans, a.Count());
		ASSERT_TRUE(dedans * 10u >= a.Count() * 9u);
	}
}

// (r4)
TEST_CASE(NKAnima, RIG3D_r4_PoidsAutomatiques) {
	const anim::NkSkinMesh &m = Mannequin(anim::NkMannequinKind::NK_HUMANOIDE_T);
	NkVector<anim::NkRigLandmark> l;
	anim::NkAutoRigOptions opt;
	ASSERT_TRUE(anim::NkRigDetectLandmarks(m, anim::NkRigTemplate::NK_HUMANOIDE, opt, l, nullptr));
	anim::NkArmature a;
	ASSERT_TRUE(anim::NkRigBuildArmature(anim::NkRigTemplate::NK_HUMANOIDE, l, opt, a));
	// Le sommet le plus a gauche (+X) : le bout de la main gauche.
	uint32 bout = 0;
	for (uint32 v = 1; v < m.VertexCount(); ++v) {
		bout = m.positions[v].x > m.positions[bout].x ? v : bout;
	}
	const anim::NkAutoWeightMethod methodes[2] = {anim::NkAutoWeightMethod::NK_CHALEUR, anim::NkAutoWeightMethod::NK_VOXELS_GEODESIQUES};
	for (uint32 q = 0; q < 2; ++q) {
		anim::NkAutoWeightOptions o;
		o.method = methodes[q];
		o.voxelResolution = 56;
		anim::NkSkinWeights w;
		anim::NkAutoWeightReport rp;
		ASSERT_TRUE(anim::NkAutoWeights(m, a, o, w, &rp));
		const anim::NkWeightCheck c = anim::NkCheckWeights(w, a);
		std::printf("  [r4] %s : %s ; %s\n", anim::NkAutoWeightMethodName(o.method), rp.message.CStr(), c.Describe(a).CStr());
		ASSERT_TRUE(c.sansPoids == 0 && c.sommesFausses == 0 && c.tropInfluences == 0 && c.maxInfluences <= 4u);
		ASSERT_TRUE(c.osOrphelins.Size() <= 2u);
		int32 fort = -1;
		float32 wf = 0.f;
		for (uint32 b = 0; b < a.Count(); ++b) {
			if (w.Get(bout, (int32)b) > wf) {
				wf = w.Get(bout, (int32)b);
				fort = (int32)b;
			}
		}
		std::printf("  [r4]   bout de la main gauche -> %s (%.3f)\n", fort >= 0 ? a.bones[(uint32)fort].name.CStr() : "?", (double)wf);
		ASSERT_TRUE(fort == a.Find("hand.L"));
		// CONTRE-EPREUVE : une somme faussee, la verification la voit.
		anim::NkSkinWeights faux = w;
		faux.Of(0)[0].weight *= 0.7f;
		ASSERT_TRUE(anim::NkCheckWeights(faux, a).sommesFausses == 1u);
	}
	// Limiter a 4 apres six influences.
	anim::NkSkinWeights w6;
	w6.Resize(1);
	for (int32 b = 0; b < 6; ++b) {
		w6.Set(0, b, 0.1f + 0.1f * (float32)b);
	}
	ASSERT_TRUE(w6.InfluenceCount(0) == 6u);
	w6.Limit(0, 4);
	ASSERT_TRUE(w6.InfluenceCount(0) == 4u && Pres(w6.Sum(0), 1.f) && w6.Get(0, 0) == 0.f && w6.Get(0, 5) > 0.f);
}

// (r5)
TEST_CASE(NKAnima, RIG3D_r5_PeintureEtAnnulation) {
	anim::NkRigDocument d;
	const anim::NkSkinMesh &mq = Mannequin(anim::NkMannequinKind::NK_HUMANOIDE_T);
	d.SetMesh(mq.positions.Data(), mq.VertexCount(), mq.indices.Data(), (uint32)mq.indices.Size());
	ASSERT_TRUE(d.DetectLandmarks(anim::NkRigTemplate::NK_HUMANOIDE));
	ASSERT_TRUE(d.BuildRig(anim::NkRigTemplate::NK_HUMANOIDE, true, anim::NkAutoWeightMethod::NK_CHALEUR));
	const anim::NkSkinWeights avant = d.weights;
	const int32 os = d.armature.Find("forearm.L");
	const int32 osM = d.armature.Find("forearm.R");
	ASSERT_TRUE(os >= 0 && osM >= 0);
	// Les sommets du haut du bras gauche (x entre 0,25 et 0,4).
	NkVector<uint32> vs;
	NkVector<float32> fs;
	for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
		const NkVec3f p = d.mesh.positions[v];
		if (p.x > 0.25f && p.x < 0.4f && p.y > 1.3f) {
			vs.PushBack(v);
			fs.PushBack(1.f);
		}
	}
	ASSERT_TRUE(vs.Size() > 10u);
	anim::NkWeightBrush br;
	br.mode = anim::NkBrushMode::NK_AJOUTER;
	br.strength = 0.3f;
	br.symmetryX = true;
	d.BeginStroke();
	uint32 changes = 0;
	for (int32 touche = 0; touche < 3; ++touche) {
		changes += d.Dab(os, br, vs.Data(), fs.Data(), (uint32)vs.Size());
	}
	d.EndStroke();
	ASSERT_TRUE(changes > 0 && d.UndoDepth() == 3u); // reperes + rig + UN trait
	const anim::NkWeightCheck c = d.CheckWeights();
	ASSERT_TRUE(c.sommesFausses == 0 && c.sansPoids == 0);
	// La symetrie : le miroir du premier sommet porte le meme poids sur forearm.R.
	const int32 mv = d.mesh.mirror[vs[0]];
	{
		uint32 apparies = 0;
		for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
			apparies += d.mesh.mirror[v] >= 0 ? 1u : 0u;
		}
		float32 best = 1e30f;
		for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
			const float32 e = Dist(d.mesh.positions[v], MiroirX(d.mesh.positions[vs[0]]));
			best = e < best ? e : best;
		}
		std::printf("  [r5] miroir : %u / %u sommets apparies ; le plus proche du miroir de %u a %.6f (diag %.4f)\n", apparies,
					d.mesh.VertexCount(), vs[0], (double)best, (double)d.mesh.Diagonal());
	}
	ASSERT_TRUE(mv >= 0);
	std::printf("  [r5] poids peint %.3f, son miroir %.3f\n", (double)d.weights.Get(vs[0], os), (double)d.weights.Get((uint32)mv, osM));
	ASSERT_TRUE(d.weights.Get(vs[0], os) > avant.Get(vs[0], os) + 0.2f);
	ASSERT_TRUE(Pres(d.weights.Get(vs[0], os), d.weights.Get((uint32)mv, osM), 0.08f));
	const anim::NkSkinWeights apres = d.weights;
	ASSERT_TRUE(d.Undo() && MemesPoids(d.weights, avant));
	ASSERT_TRUE(d.Redo() && MemesPoids(d.weights, apres));
	// CONTRE-EPREUVE : sans normalisation, les sommes ne tiennent plus et la verification le voit.
	br.autoNormalize = false;
	br.symmetryX = false;
	d.BeginStroke();
	d.Dab(os, br, vs.Data(), fs.Data(), (uint32)vs.Size());
	d.EndStroke();
	const uint32 fausses = d.CheckWeights().sommesFausses;
	std::printf("  [r5] contre-epreuve sans normalisation : %u somme(s) fausse(s) vue(s)\n", fausses);
	ASSERT_TRUE(fausses > 0u);
}

// (r6)
TEST_CASE(NKAnima, RIG3D_r6_Formes) {
	anim::NkShapeKeySet s;
	const NkVec3f base[3] = {V(0, 0, 0), V(1, 0, 0), V(0, 1, 0)};
	s.SetBasis(base, 3);
	const NkVec3f cible[3] = {V(0, 0, 2), V(1, 0.5f, 0), V(0.25f, 1, -1)};
	const int32 k = s.AddFromPositions("sourire", cible, 3);
	s.SetValue((uint32)k, 0.5f);
	NkVector<NkVec3f> r;
	s.Evaluate(r);
	bool moyenne = true;
	for (uint32 i = 0; i < 3; ++i) {
		moyenne = moyenne && r[i].x == (base[i].x + cible[i].x) * 0.5f && r[i].y == (base[i].y + cible[i].y) * 0.5f &&
				  r[i].z == (base[i].z + cible[i].z) * 0.5f;
	}
	ASSERT_TRUE(moyenne); // EXACTE, au bit pres
	// Relative a une autre forme : seule la difference s'ajoute.
	const NkVec3f large[3] = {V(0, 0, 3), V(1, 0.5f, 0), V(0.25f, 1, -1)};
	const int32 k2 = s.AddFromPositions("sourire_large", large, 3);
	s.keys[(uint32)k2].relativeTo = k;
	s.SetValue((uint32)k, 0.f);
	s.SetValue((uint32)k2, 1.f);
	s.Evaluate(r);
	ASSERT_TRUE(PresV(r[0], V(0, 0, 1)) && PresV(r[1], base[1]));
	// CONTRE-EPREUVE : relative a la mauvaise forme (la base), la difference n'est plus la meme.
	s.keys[(uint32)k2].relativeTo = 0;
	s.Evaluate(r);
	ASSERT_FALSE(PresV(r[0], V(0, 0, 1)));
	// Le PILOTE : coude a 45 deg sur [0, 90] -> 0,5.
	anim::NkShapeDriver dr;
	dr.kind = anim::NkShapeDriverKind::NK_ROTATION_OS;
	dr.bone = NkString("forearm.L");
	dr.axis = 3;
	dr.angleMin = 0.f;
	dr.angleMax = 90.f;
	s.keys[(uint32)k].driver = dr;
	const NkString noms[1] = {NkString("forearm.L")};
	const NkMat4f repos[1] = {NkMat4f::Identity()};
	const NkMat4f pose[1] = {Rot(V(1, 0, 0), 0.785398163f, V(0, 0, 0))};
	ASSERT_TRUE(s.ApplyDrivers(noms, repos, pose, 1) == 1u);
	std::printf("  [r6] pilote : angle %.3f deg -> valeur %.4f\n", (double)anim::NkShapeKeySet::DriverAngle(dr, repos[0], pose[0]),
				(double)s.keys[(uint32)k].value);
	ASSERT_TRUE(Pres(s.keys[(uint32)k].value, 0.5f, 1e-3f));
	// Le MIROIR d'une forme sur le mannequin : « clin.L » -> « clin.R ».
	anim::NkShapeKeySet f;
	const anim::NkSkinMesh &m = Mannequin(anim::NkMannequinKind::NK_HUMANOIDE_T);
	f.SetBasis(m.positions.Data(), m.VertexCount());
	const int32 g = f.AddFromBasis("clin.L");
	uint32 v0 = 0;
	for (uint32 v = 0; v < m.VertexCount(); ++v) {
		v0 = (m.positions[v].x > 0.3f && m.mirror[v] >= 0) ? v : v0;
	}
	f.keys[(uint32)g].positions[v0] = Add(f.keys[(uint32)g].positions[v0], V(0.01f, 0.02f, 0.f));
	const int32 md = f.Mirror((uint32)g, m.mirror);
	ASSERT_TRUE(md >= 0 && f.keys[(uint32)md].name == NkString("clin.R"));
	const uint32 vm = (uint32)m.mirror[v0];
	ASSERT_TRUE(PresV(Sub(f.keys[(uint32)md].positions[vm], m.positions[vm]), V(-0.01f, 0.02f, 0.f), 1e-5f));
}

// (r7)
TEST_CASE(NKAnima, RIG3D_r7_PisteDeForme) {
	anim::NkShapeKeySet s;
	const NkVec3f base[2] = {V(0, 0, 0), V(1, 0, 0)};
	const NkVec3f cible[2] = {V(0, 2, 0), V(1, 0, 4)};
	s.SetBasis(base, 2);
	const int32 k = s.AddFromPositions("sourire", cible, 2);
	anim::NkAnimationClip clip;
	clip.name = NkString("visage");
	clip.duration = 1.f;
	anim::NkAnimationTrack<float32> tr;
	tr.name = NkString("sourire");
	tr.AddKey(0.f, 0.f);
	tr.AddKey(1.f, 1.f);
	clip.morphTracks.PushBack(tr);
	clip.morphNames.PushBack(NkString("sourire"));
	ASSERT_TRUE(s.ApplyClip(clip, 0.5f) == 1u && Pres(s.keys[(uint32)k].value, 0.5f));
	NkVector<NkVec3f> r;
	s.Evaluate(r);
	ASSERT_TRUE(PresV(r[0], V(0, 1, 0)) && PresV(r[1], V(1, 0, 2)));
	// L'aller-retour .nkanim (section MRPH).
	const NkString chemin("nkanima_test_formes.nkanim");
	ASSERT_TRUE(clip.SaveBinary(chemin));
	anim::NkAnimationClip relu;
	ASSERT_TRUE(relu.LoadBinary(chemin));
	NkFile::Delete(chemin.CStr());
	ASSERT_TRUE(relu.morphTracks.Size() == 1u && relu.morphNames.Size() == 1u && relu.morphNames[0] == NkString("sourire"));
	ASSERT_TRUE(relu.morphTracks[0].KeyCount() == 2u);
	s.SetValue((uint32)k, 0.f);
	ASSERT_TRUE(s.ApplyClip(relu, 0.25f) == 1u && Pres(s.keys[(uint32)k].value, 0.25f));
	// CONTRE-EPREUVE : un nom qui ne correspond a aucune forme ne joue rien.
	relu.morphNames[0] = NkString("grimace");
	ASSERT_TRUE(s.ApplyClip(relu, 0.75f) == 0u && Pres(s.keys[(uint32)k].value, 0.25f));
}

// (r8)
TEST_CASE(NKAnima, RIG3D_r8_IKDeuxOs) {
	const NkVec3f a = V(0, 0, 0), b = V(1, 0, 0), c = V(2, 0, 0);
	const NkVec3f cible = V(1.2f, 0.9f, 0.3f), pole = V(0.5f, 0.f, -2.f);
	NkVec3f b2, c2;
	ASSERT_TRUE(anim::NkSolveTwoBoneIK(a, b, c, cible, pole, b2, c2));
	ASSERT_TRUE(PresV(c2, cible) && Pres(Dist(a, b2), 1.f) && Pres(Dist(b2, c2), 1.f));
	// Le coude est dans le plan (a, cible, pole), du cote du pole.
	const NkVec3f n = Norm(Cross(Sub(cible, a), Sub(pole, a)));
	ASSERT_TRUE(Pres(Dot(Sub(b2, a), n), 0.f, 1e-4f));
	const NkVec3f axe = Norm(Sub(cible, a));
	const NkVec3f vp = Sub(Sub(pole, a), Mul(axe, Dot(Sub(pole, a), axe)));
	const NkVec3f vb = Sub(Sub(b2, a), Mul(axe, Dot(Sub(b2, a), axe)));
	ASSERT_TRUE(Dot(vp, vb) > 0.f);
	// Hors de portee : la chaine se tend vers la cible.
	ASSERT_FALSE(anim::NkSolveTwoBoneIK(a, b, c, V(5, 0, 0), pole, b2, c2));
	ASSERT_TRUE(Pres(Dist(a, c2), 2.f, 1e-3f) && c2.x > 1.99f);
	// Sur des MATRICES : le joint du bout arrive sur la cible.
	NkMat4f w[3] = {Repere(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), a), Repere(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), b),
					Repere(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), c)};
	ASSERT_TRUE(anim::NkApplyTwoBoneIK(w, 0, 1, 2, cible, pole));
	ASSERT_TRUE(PresV(Origine(w[2]), cible, 1e-4f) && Pres(Dist(Origine(w[0]), Origine(w[1])), 1.f));
	// CONTRE-EPREUVE : avec une longueur fausse (le coude a 1,3), la cible est ratee.
	NkVec3f b3, c3;
	(void)anim::NkSolveTwoBoneIK(a, V(1.3f, 0, 0), c, cible, pole, b3, c3);
	const NkVec3f bout = Add(b3, Mul(Norm(Sub(c3, b3)), 1.f)); // la vraie longueur de l'avant-bras
	std::printf("  [r8] contre-epreuve longueur fausse : bout a %.4f de la cible\n", (double)Dist(bout, cible));
	ASSERT_TRUE(Dist(bout, cible) > 1e-2f);
}

// (r9)
TEST_CASE(NKAnima, RIG3D_r9_FormesDeVisage) {
	anim::NkRigDocument d;
	const anim::NkSkinMesh &mq = Mannequin(anim::NkMannequinKind::NK_HUMANOIDE_T);
	d.SetMesh(mq.positions.Data(), mq.VertexCount(), mq.indices.Data(), (uint32)mq.indices.Size());
	ASSERT_TRUE(d.BuildRig(anim::NkRigTemplate::NK_HUMANOIDE, false, anim::NkAutoWeightMethod::NK_CHALEUR));
	uint32 nb = 0;
	const char *const *noms = anim::NkRigDocument::FaceShapeNames(nb);
	NkVector<NkString> tous;
	for (uint32 i = 0; i < nb; ++i) {
		tous.PushBack(NkString(noms[i]));
	}
	NkString pq;
	ASSERT_TRUE(d.AddFaceShapes(tous, &pq) == nb);
	const int32 mach = d.shapes.Find("machoire_ouverte"), clin = d.shapes.Find("clin_oeil.L");
	ASSERT_TRUE(mach > 0 && clin > 0);
	const float32 neckY = d.armature.bones[(uint32)d.armature.Find("head")].head.y;
	float32 dyMin = 0.f, horsTete = 0.f, clinDroite = 0.f;
	for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
		const NkVec3f b = d.shapes.keys[0].positions[v];
		const NkVec3f dm = Sub(d.shapes.keys[(uint32)mach].positions[v], b);
		dyMin = dm.y < dyMin ? dm.y : dyMin;
		if (b.y < neckY - 0.15f) {
			horsTete += Len(dm);
		}
		if (b.x < -0.01f) {
			clinDroite += Len(Sub(d.shapes.keys[(uint32)clin].positions[v], b));
		}
	}
	std::printf("  [r9] machoire : %.4f vers le bas ; hors tete %.6f ; clin gauche cote droit %.6f\n", (double)dyMin, (double)horsTete,
				(double)clinDroite);
	ASSERT_TRUE(dyMin < -0.005f && horsTete == 0.f && clinDroite == 0.f);
	ASSERT_TRUE(d.Undo() && d.shapes.Count() == 1u);
}

// (r10) (05/10) LES CONSEILS DU RIG (NkRigSuggestions) -- ATTENDUS ECRITS AVANT :
//   a) des reperes DETECTES ne declenchent aucun conseil de placement ;
//      CONTRE-EPREUVE : le coude droit pousse de 6 cm -> « symetrie:coude », et
//      l'appliquer (le coude gauche, deplace a la main, fait reference) rend la
//      paire symetrique au 1e-4 ; UN Ctrl+Z rend le coude pousse ;
//   b) un genou sorti du volume (z + 0,3) -> « volume:genou.L », applique : dedans ;
//   c) l'armature du gabarit, bien nommee : aucun conseil de nommage ;
//      CONTRE-EPREUVE : les memes os renommes « j0 »... -> « nommage:gabarit »,
//      qui rend TOUS les noms du gabarit, en UNE operation ;
//   d) sans controles, les noms du gabarit -> « etape:controles », 7 controles ;
//   e) des sommes faussees -> « poids:normaliser », applique : poids sains ;
//   f) un os de cote dont le pendant existe sous un AUTRE nom -> le renommer.
namespace {
	int32 Conseil(const NkVector<anim::NkRigSuggestion> &l, const char *cle) {
		for (uint32 i = 0; i < (uint32)l.Size(); ++i) {
			if (l[i].key == NkString(cle)) {
				return (int32)i;
			}
		}
		return -1;
	}
	uint32 ConseilsDe(const NkVector<anim::NkRigSuggestion> &l, anim::NkRigSuggestionKind k) {
		uint32 n = 0;
		for (uint32 i = 0; i < (uint32)l.Size(); ++i) {
			n += l[i].kind == k ? 1u : 0u;
		}
		return n;
	}
} // namespace

TEST_CASE(NKAnima, RIG3D_r10_ConseilsDuRig) {
	anim::NkRigDocument d;
	const anim::NkSkinMesh &mq = Mannequin(anim::NkMannequinKind::NK_HUMANOIDE_T);
	d.SetMesh(mq.positions.Data(), mq.VertexCount(), mq.indices.Data(), (uint32)mq.indices.Size());
	anim::NkRigSuggestOptions o;
	NkVector<anim::NkRigSuggestion> l;
	anim::NkRigSuggest(d, o, l);
	ASSERT_TRUE(Conseil(l, "etape:detecter") == 0); // un maillage nu : detecter d'abord
	ASSERT_TRUE(d.DetectLandmarks(anim::NkRigTemplate::NK_HUMANOIDE));
	anim::NkRigSuggest(d, o, l);
	const uint32 placementPropre = ConseilsDe(l, anim::NkRigSuggestionKind::NK_RigSuggestionKind_Placement);
	ASSERT_TRUE(placementPropre == 0u && Conseil(l, "etape:construire") >= 0);
	// (a) Le coude droit pousse ; le gauche « deplace a la main ».
	const int32 cg = anim::NkRigFindLandmark(d.landmarks, "coude.L");
	const int32 cd = anim::NkRigFindLandmark(d.landmarks, "coude.R");
	ASSERT_TRUE(cg >= 0 && cd >= 0);
	d.landmarks[(uint32)cg].manuel = true;
	d.landmarks[(uint32)cd].position = Add(d.landmarks[(uint32)cd].position, V(0.f, 0.06f, 0.f));
	const NkVec3f pousse = d.landmarks[(uint32)cd].position;
	anim::NkRigSuggest(d, o, l);
	const int32 sy = Conseil(l, "placement:symetrie:coude");
	std::printf("  [r10a] reperes detectes : %u conseil(s) de placement ; coude pousse : conseil %s\n", placementPropre, sy >= 0 ? "VU" : "absent");
	ASSERT_TRUE(sy >= 0);
	const uint32 profondeur = d.UndoDepth();
	ASSERT_TRUE(sy >= 0 && anim::NkRigApplySuggestion(d, l[(uint32)sy]));
	ASSERT_TRUE(PresV(d.landmarks[(uint32)cd].position, MiroirX(d.landmarks[(uint32)cg].position), 1e-4f));
	ASSERT_TRUE(d.UndoDepth() == profondeur + 1u && d.Undo() && PresV(d.landmarks[(uint32)cd].position, pousse, 1e-6f));
	ASSERT_TRUE(d.Redo());
	// (b) Le genou gauche sorti du volume.
	const int32 gg = anim::NkRigFindLandmark(d.landmarks, "genou.L");
	ASSERT_TRUE(gg >= 0);
	d.landmarks[(uint32)gg].position = Add(d.landmarks[(uint32)gg].position, V(0.f, 0.f, 0.3f));
	ASSERT_FALSE(d.mesh.Contains(d.landmarks[(uint32)gg].position));
	anim::NkRigSuggest(d, o, l);
	const int32 vol = Conseil(l, "placement:volume:genou.L");
	ASSERT_TRUE(vol >= 0 && anim::NkRigApplySuggestion(d, l[(uint32)vol]));
	ASSERT_TRUE(d.mesh.Contains(d.landmarks[(uint32)gg].position));
	(void)d.Undo();
	(void)d.Undo();
	ASSERT_TRUE(d.DetectLandmarks(anim::NkRigTemplate::NK_HUMANOIDE));
	// (c) L'armature du gabarit, bien nommee.
	ASSERT_TRUE(d.BuildRig(anim::NkRigTemplate::NK_HUMANOIDE, true, anim::NkAutoWeightMethod::NK_CHALEUR));
	anim::NkRigSuggest(d, o, l);
	const uint32 nommagePropre = ConseilsDe(l, anim::NkRigSuggestionKind::NK_RigSuggestionKind_Naming);
	NkVector<NkString> attendus;
	for (uint32 b = 0; b < d.armature.Count(); ++b) {
		attendus.PushBack(d.armature.bones[b].name);
	}
	NkVector<NkString> devines;
	const uint32 nommes = anim::NkRigGuessHumanoidNames(d.armature, o.objet, devines);
	uint32 justes = 0;
	for (uint32 b = 0; b < d.armature.Count(); ++b) {
		justes += devines[b] == attendus[b] ? 1u : 0u;
		if (devines[b] != attendus[b]) {
			std::printf("        devine %s pour %s\n", devines[b].CStr(), attendus[b].CStr());
		}
	}
	std::printf("  [r10c] gabarit bien nomme : %u conseil(s) de nommage ; la structure devine %u / %u noms justes\n", nommagePropre, justes,
				d.armature.Count());
	ASSERT_TRUE(nommagePropre == 0u && nommes == d.armature.Count() && justes == d.armature.Count());
	// CONTRE-EPREUVE : des noms generiques.
	for (uint32 b = 0; b < d.armature.Count(); ++b) {
		(void)d.armature.Rename(b, NkString::Format("j%u", b).CStr());
	}
	d.controls.Clear();
	anim::NkRigSuggest(d, o, l);
	const int32 gab = Conseil(l, "nommage:gabarit");
	ASSERT_TRUE(gab >= 0 && Conseil(l, "etape:controles") < 0);
	const uint32 avant = d.UndoDepth();
	ASSERT_TRUE(gab >= 0 && anim::NkRigApplySuggestion(d, l[(uint32)gab]));
	bool tous = d.UndoDepth() == avant + 1u;
	for (uint32 b = 0; b < d.armature.Count(); ++b) {
		tous = tous && d.armature.bones[b].name == attendus[b];
	}
	ASSERT_TRUE(tous);
	// (d) Les controles du gabarit sur l'armature renommee.
	anim::NkRigSuggest(d, o, l);
	const int32 ctl = Conseil(l, "etape:controles");
	ASSERT_TRUE(ctl >= 0 && anim::NkRigApplySuggestion(d, l[(uint32)ctl]) && d.controls.Size() == 7u);
	// (e) Des sommes faussees.
	anim::NkRigSuggest(d, o, l);
	ASSERT_TRUE(Conseil(l, "poids:normaliser") < 0);
	for (uint32 v = 0; v < d.weights.VertexCount(); v += 7) {
		anim::NkSkinInfluence *s = d.weights.Of(v);
		s[0].weight *= 1.5f;
	}
	anim::NkRigSuggest(d, o, l);
	const int32 nrm = Conseil(l, "poids:normaliser");
	ASSERT_TRUE(nrm >= 0 && anim::NkRigApplySuggestion(d, l[(uint32)nrm]) && d.CheckWeights().sommesFausses == 0u);
	// (f) LE PENDANT SOUS UN AUTRE NOM (une structure qui n'est pas humanoide) :
	//     « aile.L » et son miroir « aile_x » -> renommer « aile_x » en « aile.R »,
	//     pas creer un troisieme os. CONTRE-EPREUVE : le pendant decale de 20 cm
	//     n'en est plus un -> « nommage:miroir », le conseil de creation.
	anim::NkRigDocument o2;
	o2.symetrie = false; // sinon « aile.L » cree son « aile.R » tout seul
	const int32 tronc = o2.AddBone("tronc", V(0.f, 1.f, 0.f), V(0.f, 1.4f, 0.f), -1, false);
	(void)o2.AddBone("aile.L", V(0.05f, 1.3f, 0.f), V(0.6f, 1.35f, 0.f), tronc, false);
	const int32 ax = o2.AddBone("aile_x", V(-0.05f, 1.3f, 0.f), V(-0.6f, 1.35f, 0.f), tronc, false);
	anim::NkRigSuggest(o2, o, l);
	const int32 pd = Conseil(l, "nommage:pendant:aile.L");
	ASSERT_TRUE(pd >= 0 && Conseil(l, "nommage:miroir:aile.L") < 0);
	ASSERT_TRUE(pd >= 0 && anim::NkRigApplySuggestion(o2, l[(uint32)pd]) && o2.armature.bones[(uint32)ax].name == NkString("aile.R") && o2.armature.Count() == 3u);
	(void)o2.Undo();
	o2.armature.bones[(uint32)ax].head = Add(o2.armature.bones[(uint32)ax].head, V(0.f, 0.2f, 0.f));
	o2.armature.bones[(uint32)ax].tail = Add(o2.armature.bones[(uint32)ax].tail, V(0.f, 0.2f, 0.f));
	anim::NkRigSuggest(o2, o, l);
	ASSERT_TRUE(Conseil(l, "nommage:pendant:aile.L") < 0 && Conseil(l, "nommage:miroir:aile.L") >= 0);
}
