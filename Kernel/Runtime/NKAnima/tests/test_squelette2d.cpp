// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_squelette2d.cpp — LE SQUELETTE 2D (R30, 02/10/2026).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (s1)  un modele de depart (humanoide) tient : 16 os, tous DANS LE PLAN, la
//         tete au-dessus des hanches, BrasG <-> BrasD par symetrie de nom ; les
//         quatre modeles se construisent ; CONTRE-EPREUVE : un os penche hors
//         du plan -> Validate le refuse (le mode 2D le voit)
//   (s2)  la POSE : NkBone2D <-> matrice aller-retour ; une chaine de deux os
//         tournee de 90 deg puis -90 deg pose son bout ou la geometrie a la main
//         le dit ; NkLockToPlane ramene une matrice penchee dans le plan
//   (s3)  l'IK A DEUX OS atteint sa cible (a 1e-4 pres), coude a GAUCHE ou a
//         DROITE selon le sens ; hors de portee la chaine se tend et le dit ;
//         sur une pose (NkApplyTwoBoneIK2D) la main arrive sur la cible ;
//         CONTRE-EPREUVE : le meme calcul avec une longueur fausse rate la cible
//         -> le temoin « atteint » voit ECHEC
//   (s4)  le MELANGE DE DEUX CLIPS D'OS : 0 deg et 90 deg melanges a 0,5 -> 45 deg
//         (NkBlendPose, la meme operation que les couches et les fondus) ; un
//         masque par os garde l'autre os intact ; la courbe « entree » d'une cle
//         d'os est suivie ; un controleur a deux etats fond les os
//   (s5)  le FICHIER .nkskel : os, longueurs, emplacements font l'aller-retour ;
//         un lecteur 3D (NkLoadSkeletonDef) y lit le squelette et la marque 2D ;
//         un fichier tronque est refuse ; le .nkanim relu rend ses noms d'os
//   (s6)  les EMPLACEMENTS : nom de piste <-> (emplacement, champ) ; un clip a
//         paliers change l'attache et l'ordre ; l'ordre de dessin trie stable
//
// Contre-epreuves A LA MAIN (mutation temporaire, puis remise ; 02/10) :
//   - NkSolveTwoBoneIK2D : `angle1 = base + sg * alpha` -> `base - sg * alpha`
//     => (s3) ECHEC (le bout tombe a cote de la cible) ;
//   - NkBone2DToMatrix : `m.m10 = -s * b.sy` -> `s * b.sy`
//     => (s2) ECHEC (le bout de la chaine n'est plus ou la geometrie le dit).
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKFileSystem/NkFile.h"

#include <cmath>
#include <cstring>

using namespace nkentseu;
using PK = anim::NkAnimationClip::NkPropertyKind;

namespace {
	constexpr float32 kPi = 3.14159265358979f;

	bool Pres(float32 a, float32 b, float32 tol = 1e-3f) {
		return std::fabs(a - b) < tol;
	}

	float32 Dist(const math::NkVec2f &a, const math::NkVec2f &b) {
		return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
	}

	/// Une chaine bras (1 m) -> avant-bras (1 m) -> main, couchee sur +X.
	anim::NkSkeleton2D Chaine() {
		anim::NkSkeleton2D s;
		anim::NkBone2D b;
		s.AddBone("Bras", -1, b, 1.f);
		b.x = 1.f;
		s.AddBone("AvantBras", 0, b, 1.f);
		s.AddBone("Main", 1, b, 0.2f);
		return s;
	}

	/// Le bout (la main) d'une pose.
	math::NkVec2f Main(const anim::NkSkeleton2D &s, const NkVector<anim::NkBone2D> &pose) {
		NkVector<math::NkMat4f> w;
		w.Resize(s.Count());
		s.PoseToWorld(pose.Data(), w.Data());
		return s.Head(w.Data(), 2);
	}
} // namespace

// (s1)
TEST_CASE(NKAnima, SQUELETTE2D_s1_ModelesEtModeDeux) {
	anim::NkSkeleton2D s;
	ASSERT_TRUE(anim::NkMakeSkeleton2D(anim::NkSkeleton2DTemplate::NK_HUMANOID, math::NkVec2f(-0.5f, 0.f),
									   math::NkVec2f(0.5f, 2.f), s));
	ASSERT_EQUAL(16u, s.Count());
	ASSERT_TRUE(s.Validate());
	for (uint32 k = 0; k < s.Count(); ++k) {
		ASSERT_TRUE(anim::NkIsPlanar(s.skeleton.bones[k].bindPose));
	}
	const int32 tete = s.FindBone("Tete"), hanches = s.FindBone("Hanches");
	ASSERT_TRUE(tete >= 0 && hanches >= 0);
	ASSERT_TRUE(s.skeleton.BindWorldPos((uint32)tete).y > s.skeleton.BindWorldPos((uint32)hanches).y + 0.5f);
	ASSERT_TRUE(anim::NkMirrorBoneName("BrasG") == NkString("BrasD"));
	ASSERT_TRUE(anim::NkMirrorBoneName("Torse").Empty());
	// Le bras droit est le MIROIR du gauche (x oppose, meme y).
	const math::NkVec3f g = s.skeleton.BindWorldPos((uint32)s.FindBone("MainG"));
	const math::NkVec3f d = s.skeleton.BindWorldPos((uint32)s.FindBone("MainD"));
	ASSERT_TRUE(Pres(g.x, -d.x) && Pres(g.y, d.y));
	for (uint8 t = 0; t < (uint8)anim::NkSkeleton2DTemplate::NK_COUNT; ++t) {
		anim::NkSkeleton2D m;
		ASSERT_TRUE(anim::NkMakeSkeleton2D((anim::NkSkeleton2DTemplate)t, math::NkVec2f(-1.f, 0.f), math::NkVec2f(1.f, 1.f), m));
		ASSERT_TRUE(m.Count() >= 1u && m.Validate());
	}
	// CONTRE-EPREUVE : un os qui sort du plan (rotation autour de X) -> refuse.
	anim::NkSkeleton2D penche = s;
	penche.skeleton.bones[3].bindPose = penche.skeleton.bones[3].bindPose * math::NkMat4f::RotationX(math::NkAngle(30.f));
	const char *pourquoi = nullptr;
	ASSERT_FALSE(penche.Validate(&pourquoi));
	ASSERT_TRUE(pourquoi != nullptr);
	// ... et NkLockToPlane l'y ramene.
	penche.skeleton.bones[3].bindPose = anim::NkLockToPlane(penche.skeleton.bones[3].bindPose);
	ASSERT_TRUE(penche.Validate());
}

// (s2)
TEST_CASE(NKAnima, SQUELETTE2D_s2_Pose) {
	anim::NkBone2D b;
	b.x = 0.3f;
	b.y = -0.2f;
	b.angle = 0.7f;
	b.sx = 1.5f;
	b.sy = 0.8f;
	const anim::NkBone2D r = anim::NkBone2DFromMatrix(anim::NkBone2DToMatrix(b));
	ASSERT_TRUE(Pres(r.x, b.x) && Pres(r.y, b.y) && Pres(r.angle, b.angle) && Pres(r.sx, b.sx) && Pres(r.sy, b.sy));
	const anim::NkBone2D q = anim::NkBone2DFromTRS(anim::NkBone2DToTRS(b));
	ASSERT_TRUE(Pres(q.angle, b.angle) && Pres(q.sx, b.sx));

	anim::NkSkeleton2D s = Chaine();
	ASSERT_TRUE(s.Validate());
	NkVector<anim::NkBone2D> pose;
	s.BindPose2D(pose);
	ASSERT_TRUE(Pres(Main(s, pose).x, 2.f) && Pres(Main(s, pose).y, 0.f));
	// Le bras leve de 90 deg, l'avant-bras replie de -90 deg : la main en (1, 1).
	pose[0].angle = kPi * 0.5f;
	pose[1].angle = -kPi * 0.5f;
	const math::NkVec2f m = Main(s, pose);
	ASSERT_TRUE(Pres(m.x, 1.f) && Pres(m.y, 1.f));
	// La queue d'un os : sa tete + sa longueur le long de son X.
	NkVector<math::NkMat4f> w;
	w.Resize(s.Count());
	s.PoseToWorld(pose.Data(), w.Data());
	const math::NkVec2f queue = s.Tail(w.Data(), 0);
	ASSERT_TRUE(Pres(queue.x, 0.f) && Pres(queue.y, 1.f));
	// Changer le REPOS d'un os : ses descendants suivent.
	anim::NkBone2D bras = s.BindLocal2D(0);
	bras.angle = kPi * 0.5f;
	s.SetBindLocal2D(0, bras);
	const math::NkVec3f main = s.skeleton.BindWorldPos(2);
	ASSERT_TRUE(Pres(main.x, 0.f) && Pres(main.y, 2.f));
}

// (s3)
TEST_CASE(NKAnima, SQUELETTE2D_s3_IKDeuxOs) {
	const math::NkVec2f racine(0.f, 0.f), cible(1.2f, 0.9f);
	float32 a1 = 0.f, a2 = 0.f;
	ASSERT_TRUE(anim::NkSolveTwoBoneIK2D(racine, 1.f, 1.f, cible, true, a1, a2));
	const math::NkVec2f coude(std::cos(a1), std::sin(a1));
	const math::NkVec2f bout(coude.x + std::cos(a2), coude.y + std::sin(a2));
	ASSERT_TRUE(Dist(bout, cible) < 1e-4f);
	// Coude a GAUCHE de racine -> cible (produit vectoriel positif).
	ASSERT_TRUE(cible.x * coude.y - cible.y * coude.x > 0.f);
	float32 b1 = 0.f, b2 = 0.f;
	ASSERT_TRUE(anim::NkSolveTwoBoneIK2D(racine, 1.f, 1.f, cible, false, b1, b2));
	const math::NkVec2f coude2(std::cos(b1), std::sin(b1));
	ASSERT_TRUE(cible.x * coude2.y - cible.y * coude2.x < 0.f);
	const math::NkVec2f bout2(coude2.x + std::cos(b2), coude2.y + std::sin(b2));
	ASSERT_TRUE(Dist(bout2, cible) < 1e-4f);
	// Hors de portee : tendue vers la cible, et le DIT.
	ASSERT_FALSE(anim::NkSolveTwoBoneIK2D(racine, 1.f, 1.f, math::NkVec2f(5.f, 0.f), true, a1, a2));
	ASSERT_TRUE(Pres(a1, 0.f) && Pres(a2, 0.f));

	// Sur une POSE : la main (tete de l'os 2) va sur la cible.
	anim::NkSkeleton2D s = Chaine();
	NkVector<anim::NkBone2D> pose;
	s.BindPose2D(pose);
	const math::NkVec2f vise(0.6f, -1.1f);
	ASSERT_TRUE(anim::NkApplyTwoBoneIK2D(s, pose.Data(), 1, math::NkVec2f(1.f, 0.f), vise, false));
	ASSERT_TRUE(Dist(Main(s, pose), vise) < 1e-4f);

	// CONTRE-EPREUVE : une longueur fausse (l2 = 0,7 au lieu de 1) rate la cible.
	ASSERT_TRUE(anim::NkSolveTwoBoneIK2D(racine, 1.f, 0.7f, cible, true, a1, a2));
	const math::NkVec2f faux(std::cos(a1) + std::cos(a2), std::sin(a1) + std::sin(a2)); // le vrai os fait 1 m
	ASSERT_FALSE(Dist(faux, cible) < 1e-4f);
}

// (s4)
TEST_CASE(NKAnima, SQUELETTE2D_s4_MelangeDeDeuxClipsDOs) {
	anim::NkSkeleton2D s = Chaine();
	anim::NkAnimationClip a, b;
	a.name = "repos";
	b.name = "leve";
	s.PrepareClip(a);
	s.PrepareClip(b);
	ASSERT_TRUE(a.skeletalLocal && a.boneCount == 3u && a.jointNames.Size() == 3u);
	anim::NkBone2D leve = s.BindLocal2D(0);
	leve.angle = kPi * 0.5f;
	anim::NkAddBoneKey2D(b, 0, 0.f, leve);
	anim::NkAddBoneKey2D(b, 0, 1.f, leve);
	anim::NkAnimPose pa, pb;
	anim::NkSampleClip(a, 0.5f, pa);
	anim::NkSampleClip(b, 0.5f, pb);
	ASSERT_TRUE(Pres(anim::NkBone2DFromTRS(pb.bones[0]).angle, kPi * 0.5f));
	anim::NkAnimPose r = pa;
	anim::NkBlendPose(r, pb, 0.5f);
	ASSERT_TRUE(Pres(anim::NkBone2DFromTRS(r.bones[0]).angle, kPi * 0.25f));
	// Le bras a 45 deg : la main en (cos 45 * 2, sin 45 * 2).
	NkVector<anim::NkBone2D> os;
	anim::NkPoseToBones2D(r, os);
	ASSERT_TRUE(Pres(Main(s, os).x, 1.41421f) && Pres(Main(s, os).y, 1.41421f));
	// Un masque par os : seul l'avant-bras (et ses enfants) melange -> le bras reste.
	anim::NkAddBoneKey2D(b, 1, 0.f, anim::NkBone2D{1.f, 0.f, 1.f, 1.f, 1.f}); // remplace la cle de repos
	anim::NkSampleClip(b, 0.5f, pb);
	anim::NkAnimMask masque;
	masque.name = "avant";
	anim::NkAnimMask::Entry e;
	e.path = "AvantBras";
	masque.entries.PushBack(e);
	NkVector<float32> poids;
	masque.BoneWeights(a, poids);
	ASSERT_TRUE(Pres(poids[0], 0.f) && Pres(poids[1], 1.f) && Pres(poids[2], 1.f));
	anim::NkAnimPose rm = pa;
	anim::NkBlendPose(rm, pb, 1.f, &poids);
	ASSERT_TRUE(Pres(anim::NkBone2DFromTRS(rm.bones[0]).angle, 0.f));
	ASSERT_TRUE(Pres(anim::NkBone2DFromTRS(rm.bones[1]).angle, 1.f));
	// La COURBE d'une cle d'os : « entree » (a^2) a mi-chemin -> 25 %.
	anim::NkAnimationClip c;
	s.PrepareClip(c, false);
	anim::NkAddBoneKey2D(c, 0, 0.f, anim::NkBone2D{}, anim::NkInterpMode::NK_EASE_IN);
	anim::NkAddBoneKey2D(c, 0, 1.f, anim::NkBone2D{0.f, 0.f, 1.f, 1.f, 1.f});
	// (le NLERP n'est pas lineaire en angle : 0,246 rad pour 1 rad a 25 %)
	ASSERT_TRUE(Pres(anim::NkSampleBone2D(c, 0, 0.5f).angle, 0.25f, 0.01f));
	c.boneTracks[0].SetKeyInterp(0, anim::NkInterpMode::NK_LINEAR);
	ASSERT_TRUE(Pres(anim::NkSampleBone2D(c, 0, 0.5f).angle, 0.5f)); // lineaire : la moitie
	// Un CONTROLEUR a deux etats (repos -> leve) fond les os a mi-fondu.
	anim::NkAnimController ctl;
	const int32 e0 = ctl.base.AddEmptyState("repos");
	const int32 e1 = ctl.base.AddEmptyState("leve");
	ctl.base.SetStateClipRef(e0, "repos");
	ctl.base.SetStateClipRef(e1, "leve");
	const int32 tr = ctl.base.AddTransitionEx(e0, e1, 1.f);
	ctl.base.AddCondition(tr, "lever", anim::NkAnimStateMachine::NkCondKind::BOOL_TRUE);
	anim::NkAnimationClip b2 = b;
	b2.boneTracks[1] = a.boneTracks[1];
	const anim::NkClipLookup lookup = [&](const NkString &n) -> const anim::NkAnimationClip * {
		return n == NkString("repos") ? &a : (n == NkString("leve") ? &b2 : nullptr);
	};
	anim::NkAnimControllerRuntime rt;
	anim::NkAnimPose fin;
	anim::NkAdvanceController(ctl, rt, 0.01f, lookup, fin);
	ctl.base.SetBool("lever", true);
	anim::NkAdvanceController(ctl, rt, 0.01f, lookup, fin); // la transition part
	anim::NkAdvanceController(ctl, rt, 0.49f, lookup, fin); // ~ mi-fondu
	const float32 ang = anim::NkBone2DFromTRS(fin.bones[0]).angle;
	ASSERT_TRUE(ang > 0.2f && ang < 1.35f); // entre les deux poses, ni l'une ni l'autre
}

// (s5)
TEST_CASE(NKAnima, SQUELETTE2D_s5_FichierNkskel) {
	anim::NkSkeleton2D s;
	ASSERT_TRUE(anim::NkMakeSkeleton2D(anim::NkSkeleton2DTemplate::NK_HUMANOID, math::NkVec2f(-0.4f, 0.f),
									   math::NkVec2f(0.4f, 1.8f), s));
	anim::NkSlot2DDef main;
	main.name = "Main";
	main.bone = s.FindBone("MainD");
	main.attachments.PushBack("MainOuverte");
	main.attachments.PushBack("MainFermee");
	main.attachment = 0;
	main.order = 3;
	s.slots.PushBack(main);
	const NkString chemin("nkanima_test_squelette2d.nkskel");
	ASSERT_TRUE(s.SaveBinary(chemin));
	anim::NkSkeleton2D lu;
	bool deux = false;
	ASSERT_TRUE(lu.LoadBinary(chemin, &deux));
	ASSERT_TRUE(deux);
	ASSERT_EQUAL(s.Count(), lu.Count());
	for (uint32 k = 0; k < s.Count(); ++k) {
		ASSERT_TRUE(std::strcmp(s.skeleton.bones[k].name, lu.skeleton.bones[k].name) == 0);
		ASSERT_EQUAL(s.Parent(k), lu.Parent(k));
		ASSERT_TRUE(Pres(s.lengths[k], lu.lengths[k], 1e-6f));
		ASSERT_TRUE(Pres(s.skeleton.BindWorldPos(k).x, lu.skeleton.BindWorldPos(k).x, 1e-6f));
	}
	ASSERT_EQUAL(1u, (uint32)lu.slots.Size());
	ASSERT_TRUE(lu.slots[0].name == NkString("Main") && lu.slots[0].attachments.Size() == 2u);
	ASSERT_TRUE(lu.slots[0].attachments[1] == NkString("MainFermee") && lu.slots[0].order == 3);
	// Un outil 3D y lit LE squelette, et la marque.
	anim::NkSkeletonDef def;
	bool marque = false;
	ASSERT_TRUE(anim::NkLoadSkeletonDef(chemin, def, &marque));
	ASSERT_TRUE(marque && def.Count() == s.Count());
	// Un fichier TRONQUE est refuse (sans rien ecrire a moitie).
	NkVector<nk_uint8> octets;
	s.SaveToBytes(octets);
	anim::NkSkeleton2D garde = lu;
	ASSERT_FALSE(lu.LoadFromBytes(octets.Data(), octets.Size() / 2));
	ASSERT_EQUAL(garde.Count(), lu.Count());
	NkFile::Delete(chemin.CStr());
	// Le .nkanim d'un squelette 2D rend ses noms d'os a la relecture.
	anim::NkAnimationClip clip;
	s.PrepareClip(clip);
	clip.duration = 1.f;
	ASSERT_TRUE(clip.SaveBinary("nkanima_test_squelette2d.nkanim"));
	anim::NkAnimationClip relu;
	ASSERT_TRUE(relu.LoadBinary("nkanima_test_squelette2d.nkanim"));
	ASSERT_EQUAL(s.Count(), (uint32)relu.jointNames.Size());
	ASSERT_TRUE(relu.jointNames[(uint32)s.FindBone("Tete")] == NkString("Tete"));
	NkFile::Delete("nkanima_test_squelette2d.nkanim");
}

// (s6)
TEST_CASE(NKAnima, SQUELETTE2D_s6_Emplacements) {
	NkString nom;
	bool ordre = true;
	ASSERT_TRUE(anim::NkSlot2DProperty("Main", false) == NkString("Emplacement[Main].image"));
	ASSERT_TRUE(anim::NkParseSlot2DProperty("Emplacement[Main].image", nom, ordre) && nom == NkString("Main") && !ordre);
	ASSERT_TRUE(anim::NkParseSlot2DProperty("Emplacement[Cape].ordre", nom, ordre) && nom == NkString("Cape") && ordre);
	ASSERT_FALSE(anim::NkParseSlot2DProperty("Transform.position", nom, ordre));
	anim::NkSkeleton2D s = Chaine();
	anim::NkSlot2DDef a, b;
	a.name = "Main";
	a.bone = 2;
	a.attachments.PushBack("Ouverte");
	a.attachments.PushBack("Fermee");
	a.order = 2;
	b.name = "Manche";
	b.bone = 1;
	b.order = 1;
	s.slots.PushBack(a);
	s.slots.PushBack(b);
	anim::NkAnimationClip c;
	anim::NkAnimationClip::NkPropertyTrack &pi = c.AddPropertyTrack("", anim::NkSlot2DProperty("Main", false), PK::NK_STEP);
	pi.curve.AddKey(0.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
	pi.curve.AddKey(0.5f, math::NkVec4f(1.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
	anim::NkAnimationClip::NkPropertyTrack &po = c.AddPropertyTrack("", anim::NkSlot2DProperty("Main", true), PK::NK_STEP);
	po.curve.AddKey(0.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
	c.duration = 1.f;
	anim::NkAnimPose p;
	int32 att[2] = {}, ord[2] = {};
	anim::NkSampleClip(c, 0.2f, p);
	anim::NkEvaluateSlots2D(s, p, att, ord);
	ASSERT_TRUE(att[0] == 0 && ord[0] == 0 && ord[1] == 1);
	anim::NkSampleClip(c, 0.7f, p);
	anim::NkEvaluateSlots2D(s, p, att, ord);
	ASSERT_TRUE(att[0] == 1); // la main se FERME a 0,5 s
	uint32 tri[2] = {};
	anim::NkDrawOrder2D(ord, 2, tri);
	ASSERT_TRUE(tri[0] == 0 && tri[1] == 1); // ordre 0 (Main) derriere ordre 1 (Manche)
	anim::NkAnimPose vide;
	anim::NkEvaluateSlots2D(s, vide, att, ord);
	ASSERT_TRUE(att[0] == 0 && ord[0] == 2); // le repos
	anim::NkDrawOrder2D(ord, 2, tri);
	ASSERT_TRUE(tri[0] == 1 && tri[1] == 0);
}
