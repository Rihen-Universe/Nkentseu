// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/tests/test_sequenceur_melange.cpp — le MELANGE dans le sequenceur de Noge
// (2026-10-01 soir), sur la pile de poses de NKAnima (Blend/NkAnimMix.h).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (ns1) piste Animation : deux clips qui se CHEVAUCHENT (fondu de sortie du
//         premier, d'entree du second) -- a mi-chevauchement la position est la
//         moyenne (avant : le dernier clip gagnait, x = 10)
//   (ns2) piste NLA livree : un clip Replace a influence 0,5 fond a mi-chemin ;
//         un clip Add par-dessus ajoute sa difference a sa premiere image
//   (ns3) sans registre, une piste NLA n'applique rien (la pose reste)
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/Clip/NkClipRegistry.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/Sequencer/NkSequencer.h"

using namespace nkentseu;

namespace {
	/// Un clip qui deplace l'objet de x0 a x1 sur `duree` secondes.
	anim::NkAnimationClip ClipX(const char *nom, float32 x0, float32 x1, float32 duree = 1.f) {
		anim::NkAnimationClip c;
		c.name = nom;
		c.duration = duree;
		c.position.AddKey(0.f, math::NkVec3f(x0, 0.f, 0.f));
		c.position.AddKey(duree, math::NkVec3f(x1, 0.f, 0.f));
		return c;
	}
} // namespace

TEST_CASE(NogeSequenceurMelange, ns1_AnimationFonduCroise) {
	anim::NkClipRegistry reg;
	anim::NkAnimationClip a = ClipX("a", 0.f, 0.f), b = ClipX("b", 10.f, 10.f);
	const nk_uint64 ha = reg.Register(&a), hb = reg.Register(&b);
	ecs::NkWorld monde;
	const ecs::NkEntityId e = monde.CreateEntity();
	monde.Add<ecs::NkTransform>(e, ecs::NkTransform());
	NkTrack piste;
	piste.type = NkTrackType::Animation;
	piste.entity = e;
	NkClipOnTrack &ca = piste.AddClip(ha, 0.f, 1.f);
	ca.blendOut = 0.5f;
	NkClipOnTrack &cb = piste.AddClip(hb, 0.5f, 1.f);
	cb.blendIn = 0.5f;
	piste.Evaluate(0.75f, monde, &reg); // A a 0,5, B a 0,5
	const ecs::NkTransform *t = monde.Get<ecs::NkTransform>(e);
	ASSERT_NOT_NULL(t);
	if (t == nullptr) {
		return;
	}
	ASSERT_NEAR(5.f, t->localPosition.x, 0.01f);
	piste.Evaluate(1.25f, monde, &reg); // B seul, plein
	ASSERT_NEAR(10.f, monde.Get<ecs::NkTransform>(e)->localPosition.x, 0.01f);
}

TEST_CASE(NogeSequenceurMelange, ns2_NlaRemplaceEtAjoute) {
	anim::NkClipRegistry reg;
	anim::NkAnimationClip base = ClipX("base", 8.f, 8.f), ondule = ClipX("ondule", 0.f, 2.f);
	const nk_uint64 hb = reg.Register(&base), ho = reg.Register(&ondule);
	ecs::NkWorld monde;
	const ecs::NkEntityId e = monde.CreateEntity();
	monde.Add<ecs::NkTransform>(e, ecs::NkTransform());
	NkNLATrack nla;
	nla.entity = e;
	NkNLAClip r;
	r.clipHandle = hb;
	r.startTime = 0.f;
	r.duration = 2.f;
	r.influence = 0.5f;
	nla.clips.PushBack(r);
	nla.Evaluate(0.5f, monde, &reg);
	ASSERT_NEAR(4.f, monde.Get<ecs::NkTransform>(e)->localPosition.x, 0.01f); // 0 -> 8 a moitie
	monde.Get<ecs::NkTransform>(e)->localPosition = math::NkVec3f(0.f, 0.f, 0.f);
	NkNLAClip add;
	add.clipHandle = ho;
	add.startTime = 0.f;
	add.duration = 1.f;
	add.blendType = NkNLAClip::BlendType::Add;
	nla.clips.PushBack(add);
	nla.Evaluate(0.5f, monde, &reg);
	ASSERT_NEAR(5.f, monde.Get<ecs::NkTransform>(e)->localPosition.x, 0.01f); // 4 + (1 - 0)
}

TEST_CASE(NogeSequenceurMelange, ns3_NlaSansRegistre) {
	ecs::NkWorld monde;
	const ecs::NkEntityId e = monde.CreateEntity();
	ecs::NkTransform tr;
	tr.localPosition = math::NkVec3f(3.f, 0.f, 0.f);
	monde.Add<ecs::NkTransform>(e, tr);
	NkNLATrack nla;
	nla.entity = e;
	NkNLAClip r;
	r.duration = 1.f;
	nla.clips.PushBack(r);
	nla.Evaluate(0.5f, monde);
	nla.Evaluate(0.5f, monde, nullptr);
	ASSERT_NEAR(3.f, monde.Get<ecs::NkTransform>(e)->localPosition.x, 0.001f);
}
