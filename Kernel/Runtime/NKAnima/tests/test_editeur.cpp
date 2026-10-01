// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_editeur.cpp — ce que les pages Animation et Animateur
// d'UnkenyEditor demandent au noyau (2026-10-01, feuille de route R21/R22/R35).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (e1) un clip a PISTES DE PROPRIETES fait l'aller-retour .nkanim (v4, section
//        'PROP') : cibles, noms, genres, cles, interpolations ; evaluation egale
//   (e2) un clip SANS piste de propriete s'ecrit toujours en v2 (octet 4 = 2) :
//        un lecteur d'hier le relit
//   (e3) « par paliers » : une piste NK_STEP saute, elle ne glisse pas
//   (e4) un .nkanim v4 lu par la MACHINE est refuse (c'est un clip)
//   (e5) la machine se RELIT entiere pour un editeur : reference d'un etat,
//        transitions, conditions, defauts ; SetStateClipRef
//   (e6) la disposition ('GRPH') fait l'aller-retour ; une machine SANS
//        disposition s'ecrit avec UNE section (octet pour octet comme avant)
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
	bool Pres(float32 a, float32 b) {
		return std::fabs(a - b) < 1e-4f;
	}

	anim::NkAnimationClip ClipProprietes() {
		anim::NkAnimationClip c;
		c.name = "saut";
		c.fps = 24.f;
		anim::NkAnimationClip::NkPropertyTrack &pos = c.AddPropertyTrack("", "Transform.position", PK::NK_VEC2);
		pos.curve.AddKey(0.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_LINEAR);
		pos.curve.AddKey(1.f, math::NkVec4f(2.f, 4.f, 0.f, 0.f), anim::NkInterpMode::NK_EASE_OUT);
		pos.curve.AddKey(2.f, math::NkVec4f(4.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_LINEAR);
		anim::NkAnimationClip::NkPropertyTrack &col = c.AddPropertyTrack("Bras/Main", "Sprite.couleur", PK::NK_COLOR);
		col.curve.AddKey(0.f, math::NkVec4f(1.f, 1.f, 1.f, 1.f));
		col.curve.AddKey(2.f, math::NkVec4f(1.f, 0.f, 0.f, 0.5f));
		anim::NkAnimationClip::NkPropertyTrack &vis = c.AddPropertyTrack("", "Sprite.visible", PK::NK_STEP);
		vis.curve.AddKey(0.f, math::NkVec4f(1.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
		vis.curve.AddKey(1.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
		c.RecalcDuration();
		return c;
	}
} // namespace

// (e1) + (e3)
TEST_CASE(NKAnima, EDITEUR_e1_ClipProprietesAllerRetour) {
	anim::NkAnimationClip c = ClipProprietes();
	ASSERT_TRUE(Pres(c.duration, 2.f));
	// AddPropertyTrack ne double pas une piste existante.
	c.AddPropertyTrack("", "Transform.position", PK::NK_NUMBER);
	ASSERT_EQUAL(3u, (uint32)c.propertyTracks.Size());
	ASSERT_TRUE(c.SaveBinary("nkanima_test_prop.nkanim"));
	NkVector<nk_uint8> octets = NkFile::ReadAllBytes("nkanima_test_prop.nkanim");
	ASSERT_TRUE(octets.Size() > 8u);
	ASSERT_EQUAL(4u, (uint32)octets[4]);
	anim::NkAnimationClip lu;
	ASSERT_TRUE(lu.LoadBinary("nkanima_test_prop.nkanim"));
	NkFile::Delete("nkanima_test_prop.nkanim");
	ASSERT_EQUAL(3u, (uint32)lu.propertyTracks.Size());
	anim::NkAnimationClip::NkPropertyTrack *pos = lu.FindPropertyTrack("", "Transform.position");
	anim::NkAnimationClip::NkPropertyTrack *col = lu.FindPropertyTrack("Bras/Main", "Sprite.couleur");
	anim::NkAnimationClip::NkPropertyTrack *vis = lu.FindPropertyTrack("", "Sprite.visible");
	ASSERT_TRUE(pos != nullptr && col != nullptr && vis != nullptr);
	if (pos == nullptr || col == nullptr || vis == nullptr) {
		return; // l'assertion est deja comptee : ne pas dereferencer
	}
	ASSERT_TRUE(pos->kind == PK::NK_VEC2 && col->kind == PK::NK_COLOR && vis->kind == PK::NK_STEP);
	ASSERT_EQUAL(3u, pos->curve.KeyCount());
	ASSERT_TRUE(pos->curve.GetKey(1).interp == anim::NkInterpMode::NK_EASE_OUT);
	// L'evaluation relue est celle d'origine, en plusieurs instants.
	anim::NkAnimationClip ref = ClipProprietes();
	for (int32 k = 0; k <= 20; ++k) {
		const float32 t = 0.1f * static_cast<float32>(k);
		const math::NkVec4f a = ref.propertyTracks[0].curve.Evaluate(t);
		const math::NkVec4f b = pos->curve.Evaluate(t);
		ASSERT_TRUE(Pres(a.x, b.x) && Pres(a.y, b.y));
		const math::NkVec4f ca = ref.propertyTracks[1].curve.Evaluate(t);
		const math::NkVec4f cb = col->curve.Evaluate(t);
		ASSERT_TRUE(Pres(ca.y, cb.y) && Pres(ca.w, cb.w));
	}
	// Mi-chemin de la couleur : le vert a 0,5 (lineaire).
	ASSERT_TRUE(Pres(col->curve.Evaluate(1.f).y, 0.5f));
	// (e3) Paliers : a 0,99 s encore 1, a 1 s deja 0 -- aucune valeur entre les deux.
	ASSERT_TRUE(Pres(vis->curve.Evaluate(0.5f).x, 1.f));
	ASSERT_TRUE(Pres(vis->curve.Evaluate(0.99f).x, 1.f));
	ASSERT_TRUE(Pres(vis->curve.Evaluate(1.f).x, 0.f));
}

// (e2) + (e4)
TEST_CASE(NKAnima, EDITEUR_e2_ClipSansProprietesResteV2) {
	anim::NkAnimationClip c;
	c.name = "os";
	c.ResizeBones(1);
	c.AddBoneKey(0, 0.f, math::NkMat4f::Identity());
	ASSERT_TRUE(c.SaveBinary("nkanima_test_v2.nkanim"));
	NkVector<nk_uint8> octets = NkFile::ReadAllBytes("nkanima_test_v2.nkanim");
	NkFile::Delete("nkanima_test_v2.nkanim");
	ASSERT_TRUE(octets.Size() > 8u);
	ASSERT_EQUAL(2u, (uint32)octets[4]);
	// (e4) Un clip v4 n'est pas une machine.
	anim::NkAnimationClip p = ClipProprietes();
	ASSERT_TRUE(p.SaveBinary("nkanima_test_v4.nkanim"));
	SM m;
	ASSERT_FALSE(m.LoadBinary("nkanima_test_v4.nkanim"));
	NkFile::Delete("nkanima_test_v4.nkanim");
}

// (e5) + (e6)
TEST_CASE(NKAnima, EDITEUR_e5_MachineRelueEntiere) {
	SM m;
	const int32 sol = m.AddSubMachine("Sol");
	const int32 idle = m.AddEmptyState("idle", sol);
	const int32 marche = m.AddEmptyState("marche", sol);
	const int32 saut = m.AddEmptyState("saut");
	m.SetStateTag(marche, 1);
	m.SetStateClipRef(marche, "marche_clip");
	m.DeclareParam("vitesse", SM::NkParamKind::FLOAT, 0.f);
	m.DeclareParam("auSol", SM::NkParamKind::BOOL, 1.f);
	const int32 t0 = m.AddTransitionEx(idle, marche, 0.1f, 2);
	m.AddCondition(t0, "vitesse", Kind::FLOAT_GREATER, 0.1f);
	m.AddCondition(t0, "auSol", Kind::BOOL_TRUE);
	const int32 t1 = m.AddAnyStateTransition(SM::NK_ROOT, saut, 0.f, 1);
	m.AddCondition(t1, "saut", Kind::TRIGGER);
	ASSERT_EQUAL(1, (int32)m.GetStateRefKind(marche));
	ASSERT_TRUE(m.GetStateRef(marche) == "marche_clip");
	ASSERT_EQUAL(0, (int32)m.GetStateRefKind(idle));
	ASSERT_EQUAL(2u, m.GetTransitionCount());
	ASSERT_EQUAL(idle, m.GetTransitionFrom(0));
	ASSERT_EQUAL(marche, m.GetTransitionTo(0));
	ASSERT_FALSE(m.IsAnyStateTransition(0));
	ASSERT_TRUE(m.IsAnyStateTransition(1));
	ASSERT_EQUAL(SM::NK_ROOT, m.GetTransitionScope(1));
	ASSERT_TRUE(Pres(m.GetTransitionFade(0), 0.1f));
	ASSERT_EQUAL(2, m.GetTransitionPriority(0));
	ASSERT_EQUAL(2u, m.GetConditionCount(0));
	NkString nom;
	Kind genre = Kind::BOOL_TRUE;
	float32 seuil = 0.f;
	ASSERT_TRUE(m.GetCondition(0, 0, nom, genre, seuil));
	ASSERT_TRUE(nom == "vitesse" && genre == Kind::FLOAT_GREATER && Pres(seuil, 0.1f));
	ASSERT_FALSE(m.GetCondition(0, 5, nom, genre, seuil));
	const int32 pAuSol = m.FindParam("auSol", SM::NkParamKind::BOOL);
	ASSERT_TRUE(pAuSol >= 0 && Pres(m.GetParamDefault((uint32)pAuSol), 1.f));

	// (e6) Sans disposition : UNE section, comme avant.
	NkVector<nk_uint8> sans;
	m.SaveToBytes(sans);
	ASSERT_EQUAL(1u, (uint32)sans[8]);
	m.SetStatePosition(marche, 120.f, -40.f);
	m.SetPseudoPosition(sol, 0, -200.f, 10.f);
	m.SetPseudoPosition(SM::NK_ROOT, 1, 5.f, 6.f);
	NkVector<nk_uint8> avec;
	m.SaveToBytes(avec);
	ASSERT_EQUAL(2u, (uint32)avec[8]);
	SM lu;
	ASSERT_TRUE(lu.LoadFromBytes(avec.Data(), avec.Size()));
	float32 x = 0.f, y = 0.f;
	ASSERT_TRUE(lu.GetStatePosition(marche, x, y) && Pres(x, 120.f) && Pres(y, -40.f));
	ASSERT_FALSE(lu.GetStatePosition(idle, x, y));
	ASSERT_TRUE(lu.GetPseudoPosition(sol, 0, x, y) && Pres(x, -200.f) && Pres(y, 10.f));
	ASSERT_TRUE(lu.GetPseudoPosition(SM::NK_ROOT, 1, x, y) && Pres(x, 5.f));
	ASSERT_FALSE(lu.GetPseudoPosition(SM::NK_ROOT, 0, x, y));
	// La definition, elle, est intacte : la transition tire comme avant.
	ASSERT_TRUE(lu.GetStateRef(marche) == "marche_clip");
	ASSERT_EQUAL(2u, lu.GetConditionCount(0));
	lu.SetBool("auSol", true);
	lu.SetFloat("vitesse", 1.f);
	lu.Update(0.016f);
	lu.Update(0.2f);
	ASSERT_TRUE(lu.GetCurrentPath() == "Sol/marche");
}
