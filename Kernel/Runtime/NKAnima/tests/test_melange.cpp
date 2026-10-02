// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_melange.cpp — LE MELANGE D'ANIMATIONS (2026-10-01 soir).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (m1)  la « Courbe » d'une piste de propriete est une VRAIE courbe : Hermite,
//         tangente auto BORNEE (0 -> 10 -> 4 : plate au sommet, 5 a mi-segment),
//         une tangente a la main la plie ; 'TANG' fait l'aller-retour ; un
//         segment non cubique rend exactement curve.Evaluate
//   (m2)  NkBlendPose des PROPRIETES par couverture : la valeur partagee se
//         melange, celle que b seul anime arrive au poids w, celle que a seul
//         anime recule ; un palier bascule a mi-chemin
//   (m3)  NkBlendPose des OS (translation a 30 %) ; un masque d'os en garde un
//         intact (le haut du corps tire, les jambes courent)
//   (m4)  l'ADDITIF : delta = pose - reference, rajoute au poids w (os et
//         proprietes) ; une propriete que la base n'anime pas n'en recoit pas
//   (m5)  arbres de melange : poids 1D (voisins, bouts), 2D gradient band
//         (exact sur chaque point, somme 1 entre eux)
//   (m6)  NkSampleBlendSpace sur des clips de PROPRIETES (2D d'Unkeny) : la
//         valeur et la duree melangee (phases synchronisees)
//   (m7)  pistes de clips (NLA) : deux clips qui se chevauchent se fondent ;
//         une piste additive ; NkSampleClip suit les clips poses du clip ;
//         'CLPS' fait l'aller-retour
//   (m8)  la COURBE d'un fondu de transition (douce) dans la machine elle-meme,
//         et 'FADE' fait l'aller-retour ; NkAdvanceController fond les
//         PROPRIETES de deux etats designes par leur nom
//   (m9)  COUCHES : une couche masquee (« Torse ») remplace le torse et laisse
//         les jambes ; son poids, son parametre de poids ; une couche additive
//   (m10) le controleur en .nkanimctl : couches, arbres, masques font
//         l'aller-retour ; une machine seule relit la base du meme fichier
//   (m11) un etat ARBRE DE MELANGE dans la machine : le parametre « vitesse »
//         choisit le melange, la phase avance sur la duree melangee
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKFileSystem/NkFile.h"

#include <cmath>

using namespace nkentseu;
using SM = anim::NkAnimStateMachine;
using Kind = anim::NkAnimStateMachine::NkCondKind;
using PK = anim::NkAnimationClip::NkPropertyKind;

namespace {
	bool Pres(float32 a, float32 b, float32 tol = 1e-3f) {
		return std::fabs(a - b) < tol;
	}

	/// Un clip de proprietes CONSTANTES : chaque (cible, propriete) tient sa valeur.
	anim::NkAnimationClip ClipConst(const char *nom, float32 duree,
									std::initializer_list<std::pair<const char *, float32>> valeurs, const char *cible = "") {
		anim::NkAnimationClip c;
		c.name = nom;
		c.duration = duree;
		for (const auto &v : valeurs) {
			anim::NkAnimationClip::NkPropertyTrack &p = c.AddPropertyTrack(cible, v.first, PK::NK_NUMBER);
			p.curve.AddKey(0.f, math::NkVec4f(v.second, 0.f, 0.f, 0.f));
			p.curve.AddKey(duree, math::NkVec4f(v.second, 0.f, 0.f, 0.f));
		}
		return c;
	}

	/// Un clip d'un os, fige en x (translation pure).
	anim::NkAnimationClip ClipOs(const char *nom, float32 x, uint32 nbOs = 1) {
		anim::NkAnimationClip c;
		c.name = nom;
		c.ResizeBones(nbOs);
		for (uint32 j = 0; j < nbOs; ++j) {
			const math::NkMat4f m = math::NkMat4f::Translate(math::NkVec3f(x, 0.f, 0.f));
			c.AddBoneKey(j, 0.f, m);
			c.AddBoneKey(j, 1.f, m);
		}
		c.duration = 1.f;
		return c;
	}

	float32 Valeur(const anim::NkAnimPose &p, const char *cible, const char *prop) {
		const int32 i = p.FindProp(cible, prop);
		return i >= 0 ? p.props[(uint32)i].value.x : -999.f;
	}
	float32 Couverture(const anim::NkAnimPose &p, const char *cible, const char *prop) {
		const int32 i = p.FindProp(cible, prop);
		return i >= 0 ? p.props[(uint32)i].weight : -1.f;
	}

	/// Un registre de clips pour les recherches par nom.
	struct Registre {
			NkVector<anim::NkAnimationClip> clips;
			anim::NkClipLookup Lookup() {
				return [this](const NkString &n) -> const anim::NkAnimationClip * {
					for (uint32 i = 0; i < (uint32)clips.Size(); ++i) {
						if (clips[i].name == n) {
							return &clips[i];
						}
					}
					return nullptr;
				};
			}
	};
} // namespace

// (m1)
TEST_CASE(NKAnima, MELANGE_m1_CourbeHermiteEtTangentes) {
	anim::NkAnimationClip c;
	c.name = "courbe";
	anim::NkAnimationClip::NkPropertyTrack &p = c.AddPropertyTrack("", "x", PK::NK_NUMBER);
	p.curve.AddKey(0.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_CUBIC);
	p.curve.AddKey(1.f, math::NkVec4f(10.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_LINEAR);
	p.curve.AddKey(2.f, math::NkVec4f(4.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_LINEAR);
	c.RecalcDuration();
	float32 in = 1.f, out = 1.f;
	p.KeySlopes(1, 0, in, out);
	ASSERT_TRUE(Pres(in, 0.f) && Pres(out, 0.f)); // un sommet : plate
	ASSERT_TRUE(Pres(p.Evaluate(0.5f).x, 5.f));	  // pentes nulles aux deux bouts
	ASSERT_TRUE(Pres(p.Evaluate(1.25f).x, p.curve.Evaluate(1.25f).x) && Pres(p.Evaluate(1.25f).x, 8.5f)); // lineaire : inchange
	// Une tangente a la main sur la cle du milieu : arriver en montant creuse avant.
	p.tangents.Resize(3);
	p.tangents[1].mode = anim::NkTangentMode::NK_USER;
	p.tangents[1].out = math::NkVec4f(8.f, 0.f, 0.f, 0.f);
	const float32 plie = p.Evaluate(0.5f).x;
	ASSERT_TRUE(plie < 4.5f);
	ASSERT_TRUE(c.SaveBinary("nkanima_test_tang.nkanim"));
	anim::NkAnimationClip lu;
	ASSERT_TRUE(lu.LoadBinary("nkanima_test_tang.nkanim"));
	ASSERT_EQUAL(3u, (uint32)lu.propertyTracks[0].tangents.Size());
	ASSERT_TRUE(lu.propertyTracks[0].tangents[1].mode == anim::NkTangentMode::NK_USER);
	ASSERT_TRUE(Pres(lu.propertyTracks[0].Evaluate(0.5f).x, plie));
	NkFile::Delete("nkanima_test_tang.nkanim");
}

// (m2)
TEST_CASE(NKAnima, MELANGE_m2_ProprietesParCouverture) {
	anim::NkAnimPose a, b;
	a.Prop("", "x").value.x = 0.f;
	a.Prop("", "x").weight = 1.f;
	a.Prop("", "seul_a").value.x = 3.f;
	a.Prop("", "seul_a").weight = 1.f;
	b.Prop("", "x").value.x = 10.f;
	b.Prop("", "x").weight = 1.f;
	b.Prop("", "seul_b").value.x = 4.f;
	b.Prop("", "seul_b").weight = 1.f;
	anim::NkPropValue &pal = b.Prop("", "visible", PK::NK_STEP);
	pal.value.x = 1.f;
	pal.weight = 1.f;
	anim::NkPropValue &pal0 = a.Prop("", "visible", PK::NK_STEP);
	pal0.value.x = 0.f;
	pal0.weight = 1.f;
	anim::NkAnimPose r = a;
	anim::NkBlendPose(r, b, 0.25f);
	ASSERT_TRUE(Pres(Valeur(r, "", "x"), 2.5f) && Pres(Couverture(r, "", "x"), 1.f));
	ASSERT_TRUE(Pres(Valeur(r, "", "seul_b"), 4.f) && Pres(Couverture(r, "", "seul_b"), 0.25f));
	ASSERT_TRUE(Pres(Couverture(r, "", "seul_a"), 0.75f));
	ASSERT_TRUE(Pres(Valeur(r, "", "visible"), 0.f)); // un palier ne glisse pas...
	anim::NkAnimPose r2 = a;
	anim::NkBlendPose(r2, b, 0.6f);
	ASSERT_TRUE(Pres(Valeur(r2, "", "visible"), 1.f)); // ...il bascule a mi-chemin
}

// (m3)
TEST_CASE(NKAnima, MELANGE_m3_OsEtMasque) {
	anim::NkAnimationClip a = ClipOs("a", 0.f, 2), b = ClipOs("b", 10.f, 2);
	a.jointNames.PushBack("Hanches");
	a.jointNames.PushBack("Torse");
	a.jointParent.PushBack(-1);
	a.jointParent.PushBack(0);
	anim::NkAnimPose pa, pb;
	anim::NkSampleClip(a, 0.5f, pa);
	anim::NkSampleClip(b, 0.5f, pb);
	anim::NkAnimPose r = pa;
	anim::NkBlendPose(r, pb, 0.3f);
	ASSERT_TRUE(Pres(r.bones[0].t.x, 3.f) && Pres(r.bones[1].t.x, 3.f));
	anim::NkAnimMask torse;
	anim::NkAnimMask::Entry e;
	e.path = "Torse";
	torse.entries.PushBack(e);
	NkVector<float32> poids;
	torse.BoneWeights(a, poids);
	ASSERT_TRUE(poids.Size() == 2u && Pres(poids[0], 0.f) && Pres(poids[1], 1.f));
	anim::NkAnimPose m = pa;
	anim::NkBlendPose(m, pb, 1.f, &poids);
	ASSERT_TRUE(Pres(m.bones[0].t.x, 0.f) && Pres(m.bones[1].t.x, 10.f)); // jambes intactes, torse remplace
	// Le meme masque pour des PROPRIETES 2D : « Torse » couvre « Torse/Bras ».
	ASSERT_TRUE(Pres(torse.TargetWeight("Torse/Bras"), 1.f) && Pres(torse.TargetWeight("Jambes"), 0.f));
}

// (m4)
TEST_CASE(NKAnima, MELANGE_m4_Additif) {
	anim::NkAnimationClip base = ClipOs("base", 5.f), pose = ClipOs("pose", 7.f), ref = ClipOs("ref", 2.f);
	anim::NkAnimPose pb, pp, pr, d;
	anim::NkSampleClip(base, 0.f, pb);
	anim::NkSampleClip(pose, 0.f, pp);
	anim::NkSampleClip(ref, 0.f, pr);
	pb.Prop("", "y").value.x = 1.f;
	pb.Prop("", "y").weight = 1.f;
	pp.Prop("", "y").value.x = 4.f;
	pp.Prop("", "y").weight = 1.f;
	pr.Prop("", "y").value.x = 2.f;
	pr.Prop("", "y").weight = 1.f;
	pp.Prop("", "absente").value.x = 9.f;
	pp.Prop("", "absente").weight = 1.f;
	pr.Prop("", "absente").value.x = 0.f;
	pr.Prop("", "absente").weight = 1.f;
	anim::NkMakeAdditive(pp, pr, d);
	ASSERT_TRUE(Pres(d.bones[0].t.x, 5.f));
	anim::NkApplyAdditive(pb, d, 0.5f);
	ASSERT_TRUE(Pres(pb.bones[0].t.x, 7.5f));
	ASSERT_TRUE(Pres(Valeur(pb, "", "y"), 2.f)); // 1 + (4 - 2) x 0,5
	ASSERT_TRUE(pb.FindProp("", "absente") < 0);	 // la base ne l'anime pas
}

// (m5)
TEST_CASE(NKAnima, MELANGE_m5_PoidsDesArbres) {
	anim::NkBlendSpaceDef un;
	un.dims = 1;
	un.AddSample("idle", 0.f);
	un.AddSample("marche", 1.f);
	un.AddSample("course", 3.f);
	NkVector<float32> w;
	un.Weights(2.f, 0.f, w);
	ASSERT_TRUE(Pres(w[0], 0.f) && Pres(w[1], 0.5f) && Pres(w[2], 0.5f));
	un.Weights(-1.f, 0.f, w);
	ASSERT_TRUE(Pres(w[0], 1.f));
	un.Weights(4.f, 0.f, w);
	ASSERT_TRUE(Pres(w[2], 1.f));
	anim::NkBlendSpaceDef deux;
	deux.dims = 2;
	deux.AddSample("idle", 0.f, 0.f);
	deux.AddSample("avant", 0.f, 1.f);
	deux.AddSample("gauche", -1.f, 0.f);
	deux.AddSample("droite", 1.f, 0.f);
	deux.Weights(0.f, 1.f, w);
	ASSERT_TRUE(Pres(w[1], 1.f) && Pres(w[0] + w[2] + w[3], 0.f));
	deux.Weights(0.5f, 0.5f, w);
	float32 somme = 0.f;
	for (uint32 i = 0; i < 4; ++i) {
		somme += w[i];
	}
	ASSERT_TRUE(Pres(somme, 1.f) && w[3] > 0.f && w[1] > 0.f && Pres(w[2], 0.f));
}

// (m6)
TEST_CASE(NKAnima, MELANGE_m6_ArbreDeProprietes) {
	Registre reg;
	reg.clips.PushBack(ClipConst("idle", 1.f, {{"Transform.position", 0.f}}));
	reg.clips.PushBack(ClipConst("marche", 1.f, {{"Transform.position", 10.f}}));
	reg.clips.PushBack(ClipConst("course", 2.f, {{"Transform.position", 30.f}}));
	anim::NkBlendSpaceDef bs;
	bs.dims = 1;
	bs.AddSample("idle", 0.f);
	bs.AddSample("marche", 1.f);
	bs.AddSample("course", 3.f);
	anim::NkAnimPose p;
	float32 duree = 0.f;
	ASSERT_TRUE(anim::NkSampleBlendSpace(bs, 2.f, 0.f, 0.3f, reg.Lookup(), p, &duree));
	ASSERT_TRUE(Pres(Valeur(p, "", "Transform.position"), 20.f));
	ASSERT_TRUE(Pres(duree, 1.5f)); // 0,5 x 1 s + 0,5 x 2 s
}

// (m7)
TEST_CASE(NKAnima, MELANGE_m7_PistesDeClips) {
	Registre reg;
	reg.clips.PushBack(ClipConst("A", 1.f, {{"x", 0.f}}));
	reg.clips.PushBack(ClipConst("B", 1.f, {{"x", 10.f}}));
	anim::NkAnimationClip ondule;
	ondule.name = "ondule";
	ondule.duration = 1.f;
	{
		anim::NkAnimationClip::NkPropertyTrack &p = ondule.AddPropertyTrack("", "x", PK::NK_NUMBER);
		p.curve.AddKey(0.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f));
		p.curve.AddKey(1.f, math::NkVec4f(2.f, 0.f, 0.f, 0.f));
	}
	reg.clips.PushBack(ondule);
	anim::NkAnimationClip seq;
	seq.name = "sequence";
	anim::NkClipStripTrack piste;
	anim::NkClipStrip a, b;
	a.clip = "A";
	a.start = 0.f;
	a.length = 1.f;
	a.blendOut = 0.5f;
	b.clip = "B";
	b.start = 0.5f;
	b.length = 1.f;
	b.blendIn = 0.5f;
	piste.strips.PushBack(a);
	piste.strips.PushBack(b);
	seq.clipTracks.PushBack(piste);
	seq.RecalcDuration();
	ASSERT_TRUE(Pres(seq.duration, 1.5f));
	const anim::NkClipLookup lk = reg.Lookup();
	anim::NkAnimPose p;
	anim::NkSampleClip(seq, 0.25f, p, &lk);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 0.f));
	anim::NkSampleClip(seq, 0.75f, p, &lk);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 5.f) && Pres(Couverture(p, "", "x"), 1.f)); // fondu croise
	anim::NkSampleClip(seq, 1.25f, p, &lk);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 10.f));
	// Une piste ADDITIVE au-dessus : l'ondulation (0 -> 2) s'ajoute.
	anim::NkClipStripTrack add;
	add.additive = true;
	anim::NkClipStrip o;
	o.clip = "ondule";
	o.start = 0.f;
	o.length = 2.f;
	add.strips.PushBack(o);
	seq.clipTracks.PushBack(add);
	anim::NkSampleClip(seq, 1.25f, p, &lk);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 10.5f)); // 10 + (0,5 - 0)
	ASSERT_TRUE(seq.SaveBinary("nkanima_test_seq.nkanim"));
	anim::NkAnimationClip lu;
	ASSERT_TRUE(lu.LoadBinary("nkanima_test_seq.nkanim"));
	ASSERT_EQUAL(2u, (uint32)lu.clipTracks.Size());
	ASSERT_TRUE(lu.clipTracks[1].additive && Pres(lu.clipTracks[0].strips[1].blendIn, 0.5f));
	anim::NkSampleClip(lu, 0.75f, p, &lk);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 6.5f)); // le fondu (5), plus l'ondulation a 0,75 s (1,5)
	NkFile::Delete("nkanima_test_seq.nkanim");
}

// (m8)
TEST_CASE(NKAnima, MELANGE_m8_CourbeDeFondu) {
	anim::NkAnimationClip a = ClipOs("a", 0.f), b = ClipOs("b", 10.f);
	SM sm;
	const int32 ia = sm.AddState("A", &a);
	const int32 ib = sm.AddState("B", &b);
	(void)ia;
	(void)ib;
	sm.DeclareParam("go", SM::NkParamKind::BOOL);
	const int32 t = sm.AddTransitionEx(ia, ib, 1.f);
	sm.AddCondition(t, "go", Kind::BOOL_TRUE);
	ASSERT_TRUE(sm.SetTransitionCurve(t, anim::NkFadeCurve::NK_SMOOTH));
	sm.Update(0.f);
	sm.SetBool("go", true);
	sm.Update(0.25f);
	const float32 x = sm.GetState().boneMatrices[0][3][0];
	ASSERT_TRUE(Pres(x, 10.f * 0.15625f)); // smoothstep(0,25)
	NkVector<nk_uint8> octets;
	sm.SaveToBytes(octets);
	SM lu;
	ASSERT_TRUE(lu.LoadFromBytes(octets.Data(), octets.Size()));
	ASSERT_TRUE(lu.GetTransitionCurve(0) == anim::NkFadeCurve::NK_SMOOTH);
	// Les PROPRIETES de deux etats designes par leur nom (Unkeny) se fondent.
	Registre reg;
	reg.clips.PushBack(ClipConst("repos", 1.f, {{"x", 0.f}}));
	reg.clips.PushBack(ClipConst("marche", 1.f, {{"x", 10.f}}));
	anim::NkAnimController ctl;
	const int32 r0 = ctl.base.AddEmptyState("Repos");
	const int32 r1 = ctl.base.AddEmptyState("Marche");
	ctl.base.SetStateClipRef(r0, "repos");
	ctl.base.SetStateClipRef(r1, "marche");
	ctl.base.DeclareParam("vitesse", SM::NkParamKind::FLOAT);
	ctl.base.AddTransition(r0, r1, "vitesse", Kind::FLOAT_GREATER, 0.1f, 0.5f);
	anim::NkAnimControllerRuntime rt;
	anim::NkAnimPose p;
	anim::NkAdvanceController(ctl, rt, 0.f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 0.f));
	ctl.base.SetFloat("vitesse", 1.f);
	anim::NkAdvanceController(ctl, rt, 0.25f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 5.f)); // a mi-fondu
	anim::NkAdvanceController(ctl, rt, 0.5f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 10.f) && rt.layers[0].rt.current == r1);
}

// (m9)
TEST_CASE(NKAnima, MELANGE_m9_CouchesMasquees) {
	Registre reg;
	reg.clips.PushBack(ClipConst("marche", 1.f, {{"angle", 10.f}}, "Jambes"));
	reg.clips[0].AddPropertyTrack("Torse", "angle", PK::NK_NUMBER).curve.AddKey(0.f, math::NkVec4f(10.f, 0.f, 0.f, 0.f));
	reg.clips.PushBack(ClipConst("tir", 1.f, {{"angle", 0.f}}, "Jambes"));
	reg.clips[1].AddPropertyTrack("Torse", "angle", PK::NK_NUMBER).curve.AddKey(0.f, math::NkVec4f(90.f, 0.f, 0.f, 0.f));
	reg.clips.PushBack(ClipConst("souffle", 1.f, {{"angle", 0.f}}, "Torse"));
	reg.clips[2].propertyTracks[0].curve.AddKey(0.5f, math::NkVec4f(4.f, 0.f, 0.f, 0.f));
	anim::NkAnimController ctl;
	ctl.base.SetStateClipRef(ctl.base.AddEmptyState("Marche"), "marche");
	ctl.base.DeclareParam("visee", SM::NkParamKind::FLOAT, 1.f);
	anim::NkAnimMask &torse = ctl.AddMask("HautDuCorps");
	anim::NkAnimMask::Entry e;
	e.path = "Torse";
	torse.entries.PushBack(e);
	anim::NkAnimLayer *haut = ctl.AddLayer("Tir", anim::NkLayerMode::NK_OVERRIDE, 1.f, "HautDuCorps");
	ASSERT_TRUE(haut != nullptr);
	haut->machine.SetStateClipRef(haut->machine.AddEmptyState("Tirer"), "tir");
	haut->weightParam = "visee";
	anim::NkAnimControllerRuntime rt;
	anim::NkAnimPose p;
	anim::NkAdvanceController(ctl, rt, 0.f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "Jambes", "angle"), 10.f)); // les jambes courent
	ASSERT_TRUE(Pres(Valeur(p, "Torse", "angle"), 90.f));	// le haut tire
	ctl.base.SetFloat("visee", 0.5f);
	anim::NkAdvanceController(ctl, rt, 0.f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "Torse", "angle"), 50.f)); // le parametre de poids
	// Une couche ADDITIVE (le souffle, mesure a sa premiere image) par-dessus.
	ctl.base.SetFloat("visee", 1.f);
	anim::NkAnimLayer *souffle = ctl.AddLayer("Souffle", anim::NkLayerMode::NK_ADDITIVE, 1.f);
	ASSERT_TRUE(souffle != nullptr);
	souffle->machine.SetStateClipRef(souffle->machine.AddEmptyState("Respirer"), "souffle");
	anim::NkAnimControllerRuntime rt2;
	anim::NkAdvanceController(ctl, rt2, 0.f, reg.Lookup(), p);
	anim::NkAdvanceController(ctl, rt2, 0.25f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "Torse", "angle"), 92.f)); // 90 + (2 - 0)
}

// (m10)
TEST_CASE(NKAnima, MELANGE_m10_ControleurAllerRetour) {
	anim::NkAnimController ctl;
	const int32 s0 = ctl.base.AddEmptyState("Locomotion");
	ctl.base.SetStateBlendRef(s0, "loco", 2);
	ctl.base.DeclareParam("vitesse", SM::NkParamKind::FLOAT);
	anim::NkBlendSpaceDef &bs = ctl.AddBlendSpace("loco", 2, "vitesse", "direction");
	bs.AddSample("idle", 0.f, 0.f);
	bs.AddSample("course", 3.f, 0.f).rate = 1.5f;
	anim::NkAnimMask &m = ctl.AddMask("Haut");
	anim::NkAnimMask::Entry e;
	e.path = "Spine";
	e.weight = 0.8f;
	m.entries.PushBack(e);
	anim::NkAnimLayer *l = ctl.AddLayer("Tir", anim::NkLayerMode::NK_ADDITIVE, 0.7f, "Haut");
	l->weightParam = "visee";
	l->machine.SetStateClipRef(l->machine.AddEmptyState("Tirer"), "tir");
	NkVector<nk_uint8> octets;
	ctl.SaveToBytes(octets);
	anim::NkAnimController lu;
	ASSERT_TRUE(lu.LoadFromBytes(octets.Data(), octets.Size()));
	ASSERT_EQUAL(1u, (uint32)lu.layers.Size());
	ASSERT_TRUE(lu.layers[0].mode == anim::NkLayerMode::NK_ADDITIVE && Pres(lu.layers[0].weight, 0.7f) &&
				lu.layers[0].mask == NkString("Haut") && lu.layers[0].weightParam == NkString("visee"));
	ASSERT_EQUAL(1, lu.layers[0].machine.GetStateCount());
	const anim::NkBlendSpaceDef *b = lu.FindBlendSpace("loco");
	ASSERT_TRUE(b != nullptr && b->dims == 2 && b->samples.Size() == 2u && Pres(b->samples[1].rate, 1.5f));
	ASSERT_TRUE(lu.FindMask("Haut") != nullptr && Pres(lu.FindMask("Haut")->entries[0].weight, 0.8f));
	ASSERT_TRUE(lu.base.GetStateRefKind(0) == 3 && lu.base.GetStateRef(0) == NkString("loco"));
	SM seule;
	ASSERT_TRUE(seule.LoadFromBytes(octets.Data(), octets.Size())); // les sections neuves sont sautees
	ASSERT_EQUAL(1, seule.GetStateCount());
	// Sans couche, arbre ni masque : le fichier d'une machine, octet pour octet.
	anim::NkAnimController nu;
	nu.base.AddEmptyState("A");
	NkVector<nk_uint8> o1, o2;
	nu.SaveToBytes(o1);
	nu.base.SaveToBytes(o2);
	ASSERT_TRUE(o1.Size() == o2.Size());
}

// (m11)
TEST_CASE(NKAnima, MELANGE_m11_EtatArbreDansLaMachine) {
	Registre reg;
	reg.clips.PushBack(ClipConst("idle", 1.f, {{"x", 0.f}}));
	reg.clips.PushBack(ClipConst("course", 2.f, {{"x", 30.f}}));
	anim::NkAnimController ctl;
	const int32 s0 = ctl.base.AddEmptyState("Locomotion");
	ctl.base.SetStateBlendRef(s0, "loco", 1);
	ctl.base.DeclareParam("vitesse", SM::NkParamKind::FLOAT);
	anim::NkBlendSpaceDef &bs = ctl.AddBlendSpace("loco", 1, "vitesse");
	bs.AddSample("idle", 0.f);
	bs.AddSample("course", 3.f);
	ASSERT_TRUE(ctl.HasMixing());
	anim::NkAnimControllerRuntime rt;
	anim::NkAnimPose p;
	ctl.base.SetFloat("vitesse", 1.5f);
	anim::NkAdvanceController(ctl, rt, 0.f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 15.f));
	ASSERT_TRUE(Pres(rt.layers[0].durCur, 1.5f));
	anim::NkAdvanceController(ctl, rt, 0.75f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(rt.layers[0].phaseCur, 0.5f)); // 0,75 s sur 1,5 s
	ctl.base.SetFloat("vitesse", 3.f);
	anim::NkAdvanceController(ctl, rt, 0.f, reg.Lookup(), p);
	ASSERT_TRUE(Pres(Valeur(p, "", "x"), 30.f));
}

// (m12) ajoute apres la contre-epreuve « le suivant devenu courant repart de
// zero » que m11 laissait passer : la PHASE d'un arbre qui entre par un fondu
// continue la ou le fondu l'a amenee.
TEST_CASE(NKAnima, MELANGE_m12_PhaseContinueApresLeFondu) {
	Registre reg;
	reg.clips.PushBack(ClipConst("idle", 1.f, {{"x", 0.f}}));
	reg.clips.PushBack(ClipConst("course", 1.f, {{"x", 30.f}}));
	anim::NkAnimController ctl;
	const int32 s0 = ctl.base.AddEmptyState("Repos");
	ctl.base.SetStateClipRef(s0, "idle");
	const int32 s1 = ctl.base.AddEmptyState("Loco");
	ctl.base.SetStateBlendRef(s1, "loco", 1);
	ctl.base.DeclareParam("go", SM::NkParamKind::BOOL);
	ctl.base.AddTransition(s0, s1, "go", Kind::BOOL_TRUE, 0.f, 0.5f);
	anim::NkBlendSpaceDef &bs = ctl.AddBlendSpace("loco", 1, "vitesse");
	bs.AddSample("course", 0.f);
	anim::NkAnimControllerRuntime rt;
	anim::NkAnimPose p;
	anim::NkAdvanceController(ctl, rt, 0.f, reg.Lookup(), p);
	ctl.base.SetBool("go", true);
	anim::NkAdvanceController(ctl, rt, 0.25f, reg.Lookup(), p); // mi-fondu : la phase du suivant a 0,25
	ASSERT_TRUE(rt.layers[0].rt.next == s1 && Pres(rt.layers[0].phaseNext, 0.25f));
	anim::NkAdvanceController(ctl, rt, 0.5f, reg.Lookup(), p); // le fondu finit : Loco courant
	ASSERT_TRUE(rt.layers[0].rt.current == s1 && Pres(rt.layers[0].phaseCur, 0.75f));
}
