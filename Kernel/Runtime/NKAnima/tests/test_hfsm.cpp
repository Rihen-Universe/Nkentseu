// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_hfsm.cpp — la machine a etats HIERARCHIQUE (2026-09-29).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (h0)  l'API PLATE d'avant rend les memes choix : premier AddState courant,
//         seuils float, any-state from=-1, premiere transition vraie dans
//         l'ordre d'ajout, rappels debut/fin
//   (h0b) DIFFERENTIEL : l'ancienne machine (recopiee en oracle) et la neuve,
//         sur 300 definitions plates tirees au hasard, rendent les memes etats,
//         les memes rappels et la meme pose, pas a pas
//   (h1)  sous-machine : on y ENTRE par son etat d'entree (SetEntryState), une
//         transition DEPUIS le composite tient de n'importe quel enfant, et on
//         en SORT ; chemin "Sol/marche", IsInState du composite
//   (h2)  any-state PAR NIVEAU : celle de « Air » ne tire que dedans, celle de
//         la racine de partout ; une cible deja active n'est pas re-entree
//   (h3)  un declencheur est CONSOMME au tir : deux transitions sur le meme
//         declencheur, une seule tire ; pose pendant un fondu, il attend la fin
//   (h4)  conditions combinees (ET), priorites, niveau englobant d'abord,
//         TIME_IN_STATE
//   (h5)  fondu entre FEUILLES de niveaux differents (0 -> niveau 2) : a mi-fondu
//         l'os est a mi-chemin, poids 0,5 ; apres, la feuille cible est courante
//   (h6)  parametres PARTAGES : par tous les niveaux, et entre deux machines
//   (h7)  un .nkanim v1 et un v2 ecrits A LA MAIN se relisent ; un clip
//         s'ecrit toujours en v2, octet pour octet
//   (h8)  .nkanim v3 : la machine fait l'aller-retour et se comporte pareil ;
//         une section inconnue est sautee ; v2 refuse par la machine, v3 lu
//         par le clip (clip vide)
//   (h9)  UNE definition, deux personnages (GetRuntime / SetRuntime) : chacun
//         avance comme s'il avait sa propre machine
//
// Contre-epreuves faites a la main (mutation temporaire, puis remise) : voir
// le rapport de la branche comble/nkanima-hfsm.
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKFileSystem/NkFile.h"

#include <cstring>

using namespace nkentseu;
using SM = anim::NkAnimStateMachine;
using Kind = anim::NkAnimStateMachine::NkCondKind;

namespace {
	// Un clip « legacy » (matrices directes) d'un os, fige en x : son os est
	// une translation pure, ce qui rend le fondu lisible au chiffre pres.
	anim::NkAnimationClip ClipX(const char *nom, float32 x) {
		anim::NkAnimationClip c;
		c.name = nom;
		c.ResizeBones(1);
		const math::NkMat4f m = math::NkMat4f::Translate(math::NkVec3f(x, 0.f, 0.f));
		c.AddBoneKey(0, 0.f, m);
		c.AddBoneKey(0, 1.f, m);
		c.duration = 1.f;
		return c;
	}

	float32 OsX(const SM &sm) {
		const anim::NkAnimationState &s = sm.GetState();
		return s.boneMatrices.Empty() ? -999.f : s.boneMatrices[0][3][0];
	}

	// La machine de plateforme d'Unkeny, en plus petit : Sol{idle, marche},
	// Air{saut, chute}. Les index sont rendus pour les temoins.
	struct Plateforme {
			SM sm;
			int32 sol = -1, idle = -1, marche = -1, air = -1, saut = -1, chute = -1;

			Plateforme() {
				sol = sm.AddSubMachine("Sol");
				idle = sm.AddEmptyState("idle", sol);
				marche = sm.AddEmptyState("marche", sol);
				air = sm.AddSubMachine("Air");
				saut = sm.AddEmptyState("saut", air);
				chute = sm.AddEmptyState("chute", air);
				sm.DeclareParam("auSol", SM::NkParamKind::BOOL, 1.f);
				int32 t = sm.AddTransitionEx(idle, marche, 0.f);
				sm.AddCondition(t, "vitesse", Kind::FLOAT_GREATER, 0.1f);
				t = sm.AddTransitionEx(marche, idle, 0.f);
				sm.AddCondition(t, "vitesse", Kind::FLOAT_LESS, 0.1f);
				t = sm.AddTransitionEx(sol, saut, 0.f, 1);
				sm.AddCondition(t, "saut", Kind::TRIGGER);
				t = sm.AddTransitionEx(sol, chute, 0.f);
				sm.AddCondition(t, "auSol", Kind::BOOL_FALSE);
				t = sm.AddTransitionEx(saut, chute, 0.f);
				sm.AddCondition(t, "vy", Kind::FLOAT_LESS, 0.f);
				t = sm.AddTransitionEx(air, sol, 0.f);
				sm.AddCondition(t, "auSol", Kind::BOOL_TRUE);
				sm.AddCondition(t, "vy", Kind::FLOAT_LESS, 0.01f);
			}
	};

	void PoserU32(NkVector<nk_uint8> &b, uint32 v) {
		for (int32 k = 0; k < 4; ++k) {
			b.PushBack((nk_uint8)((v >> (8 * k)) & 0xFFu));
		}
	}
	void PoserF32(NkVector<nk_uint8> &b, float32 v) {
		uint32 u = 0;
		std::memcpy(&u, &v, 4);
		PoserU32(b, u);
	}
	void PoserStr(NkVector<nk_uint8> &b, const char *s) {
		const uint32 n = (uint32)std::strlen(s);
		PoserU32(b, n);
		for (uint32 i = 0; i < n; ++i) {
			b.PushBack((nk_uint8)s[i]);
		}
	}
	// Un .nkanim ecrit OCTET PAR OCTET d'apres le commentaire de format, pas
	// par SaveBinary : un aller-retour par le meme code aurait la meme erreur
	// dans les deux sens. Un os, deux cles (x = 1 puis x = 3).
	NkVector<nk_uint8> NkanimALaMain(uint32 version) {
		NkVector<nk_uint8> b;
		PoserU32(b, 0x4E414B4Eu);
		PoserU32(b, version);
		PoserStr(b, "ancien");
		PoserF32(b, 2.f);  // duration
		PoserF32(b, 24.f); // fps
		b.PushBack(1);	   // loop
		PoserU32(b, 1);	   // boneCount
		PoserStr(b, "os0");
		b.PushBack(1); // enabled
		PoserU32(b, 2);
		for (int32 k = 0; k < 2; ++k) {
			PoserF32(b, (float32)k * 2.f);
			const math::NkMat4f m = math::NkMat4f::Translate(math::NkVec3f(k == 0 ? 1.f : 3.f, 0.f, 0.f));
			for (int32 i = 0; i < 16; ++i) {
				PoserF32(b, m.data[i]);
			}
			b.PushBack((nk_uint8)anim::NkInterpMode::NK_LINEAR);
		}
		if (version >= 2) {
			b.PushBack(1);	 // skeletalLocal
			PoserU32(b, 1);	 // jointParent
			PoserU32(b, (uint32)-1);
			PoserU32(b, 0);	 // inverseBind
			PoserU32(b, 1);	 // topo
			PoserU32(b, 0);
		}
		return b;
	}
	// ── L'ORACLE du chemin plat ────────────────────────────────────────────────
	// NkAnimStateMachine d'AVANT le 2026-09-29 (commit 07a5a050a), recopiee pour
	// le choix d'etat, le fondu et la pose des clips « legacy » : memes tables
	// de parametres (deux NkHashMap), meme boucle, meme `break`. Le temoin (h0b)
	// fait tourner les deux machines sur des definitions tirees au hasard et
	// exige les MEMES etats, les MEMES rappels et la MEME pose a chaque pas.
	class AncienneSM {
		public:
			int32 AddState(const NkString &name, const anim::NkAnimationClip *clip) {
				St s;
				s.name = name;
				s.clip = clip;
				mStates.PushBack(s);
				if (mCurrent < 0)
					mCurrent = (int32)mStates.Size() - 1;
				return (int32)mStates.Size() - 1;
			}
			void AddTransition(int32 from, int32 to, const NkString &paramName, Kind kind, float32 threshold,
							   float32 fadeDur) {
				Tr tr;
				tr.from = from;
				tr.to = to;
				tr.param = paramName;
				tr.kind = kind;
				tr.threshold = threshold;
				tr.fadeDur = fadeDur;
				mTransitions.PushBack(tr);
			}
			void SetBool(const NkString &name, bool v) {
				mBools[name] = v;
			}
			void SetFloat(const NkString &name, float32 v) {
				mFloats[name] = v;
			}
			void ForceState(int32 idx) {
				if (idx >= 0 && idx < (int32)mStates.Size()) {
					mCurrent = idx;
					mNext = -1;
					mFadeT = 0.f;
				}
			}
			int32 GetCurrentState() const {
				return mCurrent;
			}
			float32 X() const {
				return mBones.Empty() ? -999.f : mBones[0][3][0];
			}
			NkVector<NkString> journal;
			void Update(float32 dt) {
				if (mCurrent < 0 || mCurrent >= (int32)mStates.Size())
					return;
				if (mNext < 0) {
					for (uint32 i = 0; i < (uint32)mTransitions.Size(); i++) {
						const Tr &tr = mTransitions[i];
						if (tr.to < 0 || tr.to >= (int32)mStates.Size() || tr.to == mCurrent)
							continue;
						if (tr.from >= 0 && tr.from != mCurrent)
							continue;
						if (!CondTrue(tr))
							continue;
						mNext = tr.to;
						mFadeDur = tr.fadeDur > 1e-3f ? tr.fadeDur : 1e-3f;
						mFadeT = mFadeDur;
						mStates[(uint32)mNext].time = 0.f;
						journal.PushBack(mStates[(uint32)mCurrent].name + ">" + mStates[(uint32)mNext].name + ":0");
						break;
					}
				}
				Eval(mCurrent, dt, mBones);
				if (mNext >= 0) {
					Eval(mNext, dt, mNextBones);
					mFadeT -= dt;
					const float32 w = 1.f - (mFadeT > 0.f ? mFadeT / mFadeDur : 0.f);
					for (uint32 i = 0; i < (uint32)mBones.Size() && i < (uint32)mNextBones.Size(); i++) {
						for (int r = 0; r < 4; r++)
							for (int c = 0; c < 4; c++)
								mBones[i][r][c] = mBones[i][r][c] + (mNextBones[i][r][c] - mBones[i][r][c]) * w;
					}
					if (mFadeT <= 0.f) {
						const int32 prev = mCurrent;
						mCurrent = mNext;
						mNext = -1;
						journal.PushBack(mStates[(uint32)prev].name + ">" + mStates[(uint32)mCurrent].name + ":1");
					}
				}
			}

		private:
			struct St {
					NkString name;
					const anim::NkAnimationClip *clip = nullptr;
					float32 time = 0.f;
			};
			struct Tr {
					int32 from = -1;
					int32 to = -1;
					NkString param;
					Kind kind = Kind::BOOL_TRUE;
					float32 threshold = 0.f;
					float32 fadeDur = 0.25f;
			};
			bool CondTrue(const Tr &tr) const {
				if (tr.kind == Kind::BOOL_TRUE) {
					const bool *b = mBools.Find(tr.param);
					return b && *b;
				}
				const float32 *f = mFloats.Find(tr.param);
				const float32 v = f ? *f : 0.f;
				return (tr.kind == Kind::FLOAT_GREATER) ? (v > tr.threshold) : (v < tr.threshold);
			}
			void Eval(int32 idx, float32 dt, NkVector<math::NkMat4f> &out) {
				St &st = mStates[(uint32)idx];
				if (!st.clip)
					return;
				st.time += dt;
				const float32 dur = st.clip->duration > 1e-4f ? st.clip->duration : 1.f;
				st.time -= floorf(st.time / dur) * dur;
				out.Resize(st.clip->boneCount);
				for (uint32 i = 0; i < st.clip->boneCount; i++)
					out[i] = st.clip->boneTracks[i].Evaluate(st.time);
			}
			NkVector<St> mStates;
			NkVector<Tr> mTransitions;
			NkHashMap<NkString, bool> mBools;
			NkHashMap<NkString, float32> mFloats;
			int32 mCurrent = -1;
			int32 mNext = -1;
			float32 mFadeT = 0.f;
			float32 mFadeDur = 0.f;
			NkVector<math::NkMat4f> mBones, mNextBones;
	};

	// Tirage reproductible (LCG de Numerical Recipes) : une graine = un scenario.
	struct Hasard {
			uint32 e;
			explicit Hasard(uint32 graine) : e(graine) {
			}
			uint32 U() {
				e = e * 1664525u + 1013904223u;
				return e >> 8;
			}
			int32 Entre(int32 lo, int32 hi) {
				return lo + (int32)(U() % (uint32)(hi - lo + 1));
			}
			float32 F(float32 lo, float32 hi) {
				return lo + (hi - lo) * (float32)(U() % 10001u) / 10000.f;
			}
	};
} // namespace

// (h0)
TEST_CASE(NKAnima, HFSM_h0_ApiPlateInchangee) {
	anim::NkAnimationClip a = ClipX("idle", 0.f);
	anim::NkAnimationClip b = ClipX("walk", 1.f);
	anim::NkAnimationClip c = ClipX("jump", 2.f);
	SM sm;
	const int32 idle = sm.AddState("idle", &a);
	ASSERT_EQUAL(0, sm.GetCurrentState()); // le premier AddState est courant, sans Update
	const int32 walk = sm.AddState("walk", &b);
	const int32 jump = sm.AddState("jump", &c);
	ASSERT_EQUAL(0, sm.GetCurrentState());
	sm.AddTransition(idle, walk, "speed", Kind::FLOAT_GREATER, 0.5f, 0.f);
	sm.AddTransition(walk, idle, "speed", Kind::FLOAT_LESS, 0.2f, 0.f);
	sm.AddTransition(-1, jump, "jump", Kind::BOOL_TRUE, 0.f, 0.f);
	int32 debuts = 0, fins = 0;
	sm.SetTransitionCallback([&](const NkString &, const NkString &, bool fini) { (fini ? fins : debuts)++; });
	sm.Update(0.1f);
	ASSERT_EQUAL(idle, sm.GetCurrentState());
	sm.SetFloat("speed", 0.6f);
	sm.Update(0.1f);
	ASSERT_EQUAL(walk, sm.GetCurrentState());
	sm.SetFloat("speed", 0.3f); // ni < 0,2 ni > 0,5 : on reste
	sm.Update(0.1f);
	ASSERT_EQUAL(walk, sm.GetCurrentState());
	sm.SetBool("jump", true);
	sm.Update(0.1f);
	ASSERT_EQUAL(jump, sm.GetCurrentState());
	ASSERT_TRUE(sm.GetCurrentStateName() == "jump");
	sm.Update(0.1f); // cible deja courante : l'any-state ne re-entre pas
	ASSERT_EQUAL(jump, sm.GetCurrentState());
	ASSERT_EQUAL(2, debuts);
	ASSERT_EQUAL(2, fins);
	ASSERT_NEAR(2.f, OsX(sm), 1e-4f);
	sm.SetBool("jump", false);
	sm.SetFloat("speed", 0.1f); // walk -> idle ne part pas de jump
	sm.Update(0.1f);
	ASSERT_EQUAL(jump, sm.GetCurrentState());
	sm.ForceState(idle);
	ASSERT_EQUAL(idle, sm.GetCurrentState());

	// Deux any-state vraies ensemble : la PREMIERE ajoutee gagne. Et l'ancienne
	// regle jusque dans son travers : la cible atteinte, la premiere saute (sa
	// cible est courante) et la seconde tire — elles alternent. Ce n'est pas
	// une qualite, c'est ce que faisait le code : le chemin plat le garde.
	SM n;
	n.AddState("a", &a);
	const int32 nb = n.AddState("b", &b);
	const int32 nc = n.AddState("c", &c);
	n.AddTransition(-1, nb, "x", Kind::BOOL_TRUE, 0.f, 0.f);
	n.AddTransition(-1, nc, "x", Kind::BOOL_TRUE, 0.f, 0.f);
	n.SetBool("x", true);
	n.Update(0.1f);
	ASSERT_EQUAL(nb, n.GetCurrentState());
	n.Update(0.1f);
	ASSERT_EQUAL(nc, n.GetCurrentState());
	n.Update(0.1f);
	ASSERT_EQUAL(nb, n.GetCurrentState());
	// Un meme nom en bool ET en float : deux parametres (comme les deux tables d'avant).
	sm.SetBool("jump", true);
	sm.SetFloat("jump", 7.f);
	ASSERT_TRUE(sm.GetBool("jump"));
	ASSERT_NEAR(7.f, sm.GetFloat("jump"), 1e-6f);
	ASSERT_NEAR(0.f, sm.GetFloat("absent"), 1e-6f);
}

// (h0b) le chemin plat, DIFFERENTIEL contre l'oracle : 300 machines tirees au
// hasard (2 a 5 etats, 1 a 8 transitions, any-state, trois conditions, fondus
// nuls ou non), 80 pas chacune, parametres et ForceState au hasard.
TEST_CASE(NKAnima, HFSM_h0b_PlatDifferentielContreAncien) {
	static const char *kNoms[3] = {"p", "q", "r"};
	int32 ecarts = 0;
	int32 bascules = 0;
	for (uint32 graine = 1; graine <= 300; ++graine) {
		Hasard h(graine);
		const int32 nEtats = h.Entre(2, 5);
		NkVector<anim::NkAnimationClip> clips;
		clips.Resize((usize)nEtats);
		AncienneSM ancien;
		SM neuf;
		NkVector<NkString> journal;
		neuf.SetTransitionCallback([&](const NkString &de, const NkString &vers, bool fini) {
			journal.PushBack(de + ">" + vers + (fini ? ":1" : ":0"));
		});
		for (int32 i = 0; i < nEtats; ++i) {
			clips[(usize)i] = ClipX("c", (float32)(i * 3));
			const NkString nom = NkString::Format("e%d", i);
			ancien.AddState(nom, &clips[(usize)i]);
			neuf.AddState(nom, &clips[(usize)i]);
		}
		const int32 nTr = h.Entre(1, 8);
		for (int32 t = 0; t < nTr; ++t) {
			const int32 from = h.Entre(-1, nEtats - 1);
			const int32 to = h.Entre(0, nEtats - 1);
			const Kind k = (Kind)h.Entre(0, 2);
			const NkString param(kNoms[h.Entre(0, 2)]);
			const float32 seuil = h.F(-1.f, 1.f);
			const float32 fondu = h.Entre(0, 2) == 0 ? 0.f : h.F(0.05f, 0.4f);
			ancien.AddTransition(from, to, param, k, seuil, fondu);
			neuf.AddTransition(from, to, param, k, seuil, fondu);
		}
		for (int32 pas = 0; pas < 80; ++pas) {
			const int32 geste = h.Entre(0, 9);
			const NkString param(kNoms[h.Entre(0, 2)]);
			if (geste <= 3) {
				const float32 v = h.F(-1.5f, 1.5f);
				ancien.SetFloat(param, v);
				neuf.SetFloat(param, v);
			} else if (geste <= 6) {
				const bool v = h.Entre(0, 1) == 1;
				ancien.SetBool(param, v);
				neuf.SetBool(param, v);
			} else if (geste == 7) {
				const int32 e = h.Entre(-1, nEtats);
				ancien.ForceState(e);
				neuf.ForceState(e);
			}
			const float32 dt = h.F(0.01f, 0.2f);
			ancien.Update(dt);
			neuf.Update(dt);
			const float32 dx = ancien.X() - OsX(neuf);
			if (ancien.GetCurrentState() != neuf.GetCurrentState() || dx > 1e-4f || dx < -1e-4f) {
				++ecarts;
			}
		}
		bool memeJournal = journal.Size() == ancien.journal.Size();
		for (usize i = 0; memeJournal && i < journal.Size(); ++i) {
			memeJournal = journal[i] == ancien.journal[i];
		}
		ecarts += memeJournal ? 0 : 1;
		bascules += (int32)journal.Size();
	}
	ASSERT_EQUAL(0, ecarts);
	// Le temoin n'est pas vide : les scenarios basculent vraiment.
	ASSERT_GREATER(bascules, 1000);
}

// (h1)
TEST_CASE(NKAnima, HFSM_h1_SousMachineEntreeSortie) {
	SM sm;
	const int32 dehors = sm.AddEmptyState("dehors");
	const int32 sol = sm.AddSubMachine("Sol");
	const int32 idle = sm.AddEmptyState("idle", sol);
	const int32 marche = sm.AddEmptyState("marche", sol);
	ASSERT_EQUAL(1, sm.GetStateDepth(idle));
	ASSERT_EQUAL(idle, sm.GetEntryState(sol)); // par defaut : le premier enfant
	ASSERT_TRUE(sm.SetEntryState(sol, marche));
	ASSERT_FALSE(sm.SetEntryState(sol, dehors)); // pas un enfant : refuse
	ASSERT_EQUAL(-1, sm.AddEmptyState("orphelin", dehors)); // parent qui n'est pas une sous-machine
	int32 t = sm.AddTransitionEx(dehors, sol, 0.f);
	sm.AddCondition(t, "entrer", Kind::BOOL_TRUE);
	t = sm.AddTransitionEx(marche, idle, 0.f);
	sm.AddCondition(t, "arret", Kind::BOOL_TRUE);
	t = sm.AddTransitionEx(sol, dehors, 0.f); // DEPUIS le composite
	sm.AddCondition(t, "sortir", Kind::BOOL_TRUE);

	sm.Update(0.1f);
	ASSERT_EQUAL(dehors, sm.GetCurrentState());
	sm.SetBool("entrer", true);
	sm.Update(0.1f);
	ASSERT_EQUAL(marche, sm.GetCurrentState()); // entree explicite, pas le premier enfant
	ASSERT_TRUE(sm.GetCurrentPath() == "Sol/marche");
	ASSERT_TRUE(sm.IsInState(sol));
	ASSERT_FALSE(sm.IsInState(dehors));
	sm.SetBool("entrer", false);
	sm.SetBool("arret", true);
	sm.Update(0.1f);
	ASSERT_EQUAL(idle, sm.GetCurrentState()); // transition INTERIEURE
	sm.SetBool("sortir", true);
	sm.Update(0.1f);
	ASSERT_EQUAL(dehors, sm.GetCurrentState()); // sortie depuis un enfant quelconque
	ASSERT_FALSE(sm.IsInState(sol));
	ASSERT_EQUAL(sol, sm.FindState("Sol"));
	ASSERT_EQUAL(marche, sm.FindState("Sol/marche"));
	ASSERT_EQUAL(-1, sm.FindState("Air/marche"));
	// ForceState sur un composite : on y entre par son entree.
	sm.ForceState(sol);
	ASSERT_EQUAL(marche, sm.GetCurrentState());
}

// (h2)
TEST_CASE(NKAnima, HFSM_h2_AnyStateParNiveau) {
	Plateforme p;
	SM &sm = p.sm;
	const int32 mort = sm.AddEmptyState("mort");
	const int32 tAir = sm.AddAnyStateTransition(p.air, p.chute, 0.f);
	sm.AddCondition(tAir, "lourd", Kind::BOOL_TRUE);
	const int32 tRacine = sm.AddAnyStateTransition(SM::NK_ROOT, mort, 0.f);
	sm.AddCondition(tRacine, "mort", Kind::TRIGGER);
	ASSERT_EQUAL(-1, sm.AddAnyStateTransition(p.idle, mort, 0.f)); // une feuille n'est pas une portee
	int32 bascules = 0;
	sm.SetTransitionCallback([&](const NkString &, const NkString &, bool fini) { bascules += fini ? 0 : 1; });

	sm.SetBool("lourd", true);
	sm.Update(0.1f);
	ASSERT_EQUAL(p.idle, sm.GetCurrentState()); // any-state de « Air » : muette hors de Air
	sm.SetTrigger("saut");
	sm.Update(0.1f);
	ASSERT_EQUAL(p.saut, sm.GetCurrentState());
	sm.SetFloat("vy", 3.f); // ne tombe pas encore : c'est l'any-state qui doit jouer
	sm.Update(0.1f);
	ASSERT_EQUAL(p.chute, sm.GetCurrentState()); // any-state de Air, depuis saut
	const int32 avant = bascules;
	sm.Update(0.1f);
	sm.Update(0.1f);
	ASSERT_EQUAL(avant, bascules); // chute deja active : pas re-entree a chaque trame
	sm.SetTrigger("mort");
	sm.Update(0.1f);
	ASSERT_EQUAL(mort, sm.GetCurrentState()); // any-state de la racine : de partout
}

// (h3)
TEST_CASE(NKAnima, HFSM_h3_DeclencheurConsommeUneFois) {
	anim::NkAnimationClip ca = ClipX("a", 0.f);
	anim::NkAnimationClip cb = ClipX("b", 1.f);
	SM sm;
	const int32 a = sm.AddState("a", &ca);
	const int32 b = sm.AddState("b", &cb);
	int32 t = sm.AddTransitionEx(a, b, 0.f);
	sm.AddCondition(t, "coup", Kind::TRIGGER);
	t = sm.AddTransitionEx(b, a, 0.f);
	sm.AddCondition(t, "coup", Kind::TRIGGER);
	sm.SetTrigger("coup");
	ASSERT_TRUE(sm.GetTrigger("coup"));
	sm.Update(0.1f);
	ASSERT_EQUAL(b, sm.GetCurrentState());
	ASSERT_FALSE(sm.GetTrigger("coup")); // consomme par la transition qui a tire
	sm.Update(0.1f);
	sm.Update(0.1f);
	ASSERT_EQUAL(b, sm.GetCurrentState()); // l'autre transition n'a rien recu

	// Pose PENDANT un fondu : il n'est pas perdu, il tire a la fin du fondu.
	const int32 lent = sm.AddTransitionEx(b, a, 0.5f, 5);
	sm.AddCondition(lent, "lent", Kind::TRIGGER);
	sm.SetTrigger("lent");
	sm.Update(0.1f); // debut du fondu b -> a
	ASSERT_EQUAL(a, sm.GetNextState());
	sm.SetTrigger("coup");
	sm.Update(0.2f);
	sm.Update(0.3f); // fin du fondu : a est courant
	ASSERT_EQUAL(a, sm.GetCurrentState());
	ASSERT_TRUE(sm.GetTrigger("coup")); // toujours pose : pas de transition pendant le fondu
	sm.Update(0.1f);
	ASSERT_EQUAL(b, sm.GetCurrentState());
	ASSERT_FALSE(sm.GetTrigger("coup"));
	// ResetTrigger baisse sans tirer.
	sm.SetTrigger("coup");
	sm.ResetTrigger("coup");
	sm.Update(0.1f);
	ASSERT_EQUAL(b, sm.GetCurrentState());
}

// (h4)
TEST_CASE(NKAnima, HFSM_h4_ConditionsPrioritesTemps) {
	Plateforme p;
	SM &sm = p.sm;
	sm.Update(0.1f);
	ASSERT_EQUAL(p.idle, sm.GetCurrentState());
	// Air -> Sol veut auSol ET vy < 0,01 : en l'air on descend vers la chute.
	sm.SetBool("auSol", false);
	sm.Update(0.1f);
	ASSERT_EQUAL(p.chute, sm.GetCurrentState());
	sm.SetBool("auSol", true);
	sm.SetFloat("vy", 2.f); // au sol mais encore en montee : la conjonction ne tient pas
	sm.Update(0.1f);
	ASSERT_EQUAL(p.chute, sm.GetCurrentState());
	sm.SetFloat("vy", 0.f);
	sm.Update(0.1f);
	ASSERT_EQUAL(p.idle, sm.GetCurrentState()); // entree de Sol

	// Priorite : saut (1) l'emporte sur auSol = false (0), ajoutee avant elle.
	sm.SetBool("auSol", false);
	sm.SetTrigger("saut");
	sm.Update(0.1f);
	ASSERT_EQUAL(p.saut, sm.GetCurrentState());

	// A priorite egale, le niveau ENGLOBANT d'abord, meme ajoute apres.
	SM n;
	const int32 g = n.AddSubMachine("G");
	const int32 x = n.AddEmptyState("x", g);
	const int32 y = n.AddEmptyState("y", g);
	const int32 z = n.AddEmptyState("z");
	int32 t = n.AddTransitionEx(x, y, 0.f);
	n.AddCondition(t, "go", Kind::BOOL_TRUE);
	t = n.AddTransitionEx(g, z, 0.f);
	n.AddCondition(t, "go", Kind::BOOL_TRUE);
	n.SetBool("go", true);
	n.Update(0.1f);
	ASSERT_EQUAL(z, n.GetCurrentState());

	// TIME_IN_STATE : l'etat source compte depuis son ENTREE, et un parent
	// garde son temps quand on change d'enfant.
	SM m;
	const int32 grp = m.AddSubMachine("grp");
	const int32 u = m.AddEmptyState("u", grp);
	const int32 v = m.AddEmptyState("v", grp);
	const int32 w = m.AddEmptyState("w");
	t = m.AddTransitionEx(u, v, 0.f);
	// 0,25 et non 0,3 : trois pas de 0,1 f font 0,3 a un ulp pres, et un seuil
	// pose sur la frontiere teste l'arrondi, pas la machine.
	m.AddCondition(t, "", Kind::TIME_IN_STATE, 0.25f);
	t = m.AddTransitionEx(grp, w, 0.f);
	m.AddCondition(t, "", Kind::TIME_IN_STATE, 0.55f);
	m.Update(0.1f);
	m.Update(0.1f);
	ASSERT_EQUAL(u, m.GetCurrentState());
	m.Update(0.1f); // 0,3 s dans u
	ASSERT_EQUAL(v, m.GetCurrentState());
	ASSERT_NEAR(0.3f, m.GetTimeInState(grp), 1e-4f); // grp n'a pas ete re-entre
	m.Update(0.1f);
	m.Update(0.1f);
	ASSERT_EQUAL(v, m.GetCurrentState());
	m.Update(0.1f); // 0,6 s dans grp
	ASSERT_EQUAL(w, m.GetCurrentState());
	ASSERT_NEAR(-1.f, m.GetTimeInState(grp), 1e-6f);
}

// (h5)
TEST_CASE(NKAnima, HFSM_h5_FonduEntreNiveaux) {
	anim::NkAnimationClip c0 = ClipX("sol", 0.f);
	anim::NkAnimationClip c2 = ClipX("haut", 10.f);
	SM sm;
	const int32 bas = sm.AddState("bas", &c0);			 // niveau 0
	const int32 air = sm.AddSubMachine("Air");			 // niveau 0
	const int32 cime = sm.AddSubMachine("Cime", air);	 // niveau 1
	const int32 haut = sm.AddState(cime, "haut", &c2);	 // niveau 2
	ASSERT_EQUAL(2, sm.GetStateDepth(haut));
	const int32 t = sm.AddTransitionEx(bas, air, 1.f);
	sm.AddCondition(t, "monter", Kind::BOOL_TRUE);
	sm.Update(0.25f);
	ASSERT_NEAR(0.f, OsX(sm), 1e-4f);
	sm.SetBool("monter", true);
	sm.Update(0.25f); // declenche : 0,25 s de fondu
	ASSERT_EQUAL(haut, sm.GetNextState());
	ASSERT_NEAR(0.25f, sm.GetFadeWeight(), 1e-4f);
	sm.Update(0.25f); // mi-fondu
	ASSERT_NEAR(0.5f, sm.GetFadeWeight(), 1e-4f);
	ASSERT_NEAR(5.f, OsX(sm), 1e-3f); // l'os a mi-chemin entre les deux FEUILLES
	ASSERT_EQUAL(bas, sm.GetCurrentState());
	sm.Update(0.25f);
	sm.Update(0.25f);
	ASSERT_EQUAL(haut, sm.GetCurrentState());
	ASSERT_EQUAL(-1, sm.GetNextState());
	ASSERT_NEAR(0.f, sm.GetFadeWeight(), 1e-6f);
	ASSERT_NEAR(10.f, OsX(sm), 1e-3f);
	ASSERT_TRUE(sm.GetCurrentPath() == "Air/Cime/haut");
}

// (h6)
TEST_CASE(NKAnima, HFSM_h6_ParametresPartages) {
	// Par tous les niveaux : un parametre pose une fois est lu par une
	// transition de niveau 2 (la sous-machine n'a pas de table a elle).
	SM sm;
	const int32 a = sm.AddSubMachine("A");
	const int32 b = sm.AddSubMachine("B", a);
	const int32 p = sm.AddEmptyState("p", b);
	const int32 q = sm.AddEmptyState("q", b);
	const int32 t = sm.AddTransitionEx(p, q, 0.f);
	sm.AddCondition(t, "k", Kind::FLOAT_GREATER, 1.f);
	sm.SetFloat("k", 2.f);
	sm.Update(0.1f);
	ASSERT_EQUAL(q, sm.GetCurrentState());
	ASSERT_EQUAL(1u, sm.GetParamCount());

	// Entre machines : la couche « haut du corps » lit les parametres de la
	// locomotion.
	SM loco;
	loco.AddEmptyState("x");
	SM haut;
	const int32 h0 = haut.AddEmptyState("repos");
	const int32 h1 = haut.AddEmptyState("vise");
	const int32 th = haut.AddTransitionEx(h0, h1, 0.f);
	haut.AddCondition(th, "viser", Kind::TRIGGER);
	ASSERT_TRUE(haut.ShareParametersWith(&loco));
	ASSERT_FALSE(loco.ShareParametersWith(&haut)); // cycle refuse
	loco.SetTrigger("viser");
	haut.Update(0.1f);
	ASSERT_EQUAL(h1, haut.GetCurrentState());
	ASSERT_FALSE(loco.GetTrigger("viser")); // consomme chez son proprietaire
}

// (h7)
TEST_CASE(NKAnima, NKANIM_h7_AncienFormatRelu) {
	const char *chemins[2] = {"nkanima_test_v1.nkanim", "nkanima_test_v2.nkanim"};
	for (uint32 v = 1; v <= 2; ++v) {
		const NkVector<nk_uint8> octets = NkanimALaMain(v);
		ASSERT_TRUE(NkFile::WriteAllBytes(chemins[v - 1], octets));
		anim::NkAnimationClip c;
		ASSERT_TRUE(c.LoadBinary(chemins[v - 1]));
		ASSERT_TRUE(c.name == "ancien");
		ASSERT_NEAR(2.f, c.duration, 1e-6f);
		ASSERT_NEAR(24.f, c.fps, 1e-6f);
		ASSERT_EQUAL(1u, c.boneCount);
		ASSERT_EQUAL(2u, c.boneTracks[0].KeyCount());
		ASSERT_NEAR(2.f, c.boneTracks[0].Evaluate(1.f)[3][0], 1e-4f); // mi-chemin entre 1 et 3
		ASSERT_EQUAL(v >= 2, c.skeletalLocal);
		ASSERT_EQUAL(v >= 2 ? 1u : 0u, (uint32)c.jointTopo.Size());
		// Un clip s'ECRIT toujours en v2 : reecrit, le v2 revient octet pour octet.
		if (v == 2) {
			ASSERT_TRUE(c.SaveBinary("nkanima_test_v2_reecrit.nkanim"));
			const NkVector<nk_uint8> relu = NkFile::ReadAllBytes("nkanima_test_v2_reecrit.nkanim");
			bool memes = relu.Size() == octets.Size();
			for (usize i = 0; memes && i < relu.Size(); ++i) {
				memes = relu[i] == octets[i];
			}
			ASSERT_TRUE(memes);
			NkFile::Delete("nkanima_test_v2_reecrit.nkanim");
		}
		NkFile::Delete(chemins[v - 1]);
	}
	// La machine refuse un clip seul, proprement.
	const NkVector<nk_uint8> v2 = NkanimALaMain(2);
	SM sm;
	ASSERT_FALSE(sm.LoadFromBytes(v2.Data(), v2.Size()));
}

// (h8)
TEST_CASE(NKAnima, NKANIM_h8_MachineV3AllerRetour) {
	anim::NkAnimationClip ca = ClipX("clipA", 0.f);
	anim::NkAnimationClip cb = ClipX("clipB", 4.f);
	SM src;
	const int32 grp = src.AddSubMachine("grp");
	src.AddState(grp, "a", &ca);
	const int32 b = src.AddState(grp, "b", &cb);
	src.SetEntryState(grp, b);
	src.SetStateTag(b, 7);
	const int32 z = src.AddEmptyState("z");
	src.DeclareParam("seuil", SM::NkParamKind::FLOAT, 0.25f);
	int32 t = src.AddTransitionEx(grp, z, 0.f, 3);
	src.AddCondition(t, "go", Kind::TRIGGER);
	src.AddCondition(t, "seuil", Kind::FLOAT_GREATER, 0.5f);
	t = src.AddAnyStateTransition(grp, b, 0.1f, 2);
	src.AddCondition(t, "retour", Kind::BOOL_FALSE);

	NkVector<nk_uint8> octets;
	src.SaveToBytes(octets);
	// Une section INCONNUE glissee avant 'HFSM' : un lecteur d'aujourd'hui doit
	// la sauter. Le corps de clip vide fait 30 octets : le compteur de
	// sections est a 8 + 30.
	NkVector<nk_uint8> avecInconnue;
	for (usize i = 0; i < 38; ++i) {
		avecInconnue.PushBack(octets[i]);
	}
	PoserU32(avecInconnue, 2);
	PoserU32(avecInconnue, 0x534F4D45u); // 'EMOS'
	PoserU32(avecInconnue, 3);
	avecInconnue.PushBack(9);
	avecInconnue.PushBack(9);
	avecInconnue.PushBack(9);
	for (usize i = 42; i < octets.Size(); ++i) {
		avecInconnue.PushBack(octets[i]);
	}
	SM::NkResolver res;
	res.clip = [&](const NkString &n) -> const anim::NkAnimationClip * {
		return n == "clipA" ? &ca : (n == "clipB" ? &cb : nullptr);
	};
	SM lu;
	ASSERT_TRUE(lu.LoadFromBytes(avecInconnue.Data(), avecInconnue.Size(), res));
	ASSERT_EQUAL(src.GetStateCount(), lu.GetStateCount());
	ASSERT_EQUAL(src.GetTransitionCount(), lu.GetTransitionCount());
	ASSERT_EQUAL(b, lu.GetEntryState(grp));
	ASSERT_EQUAL(7, lu.GetStateTag(b));
	ASSERT_EQUAL(b, lu.GetCurrentState());
	ASSERT_EQUAL(src.GetParamCount(), lu.GetParamCount());
	ASSERT_NEAR(0.25f, lu.GetFloat("seuil"), 1e-6f); // le DEFAUT est relu
	lu.Update(0.1f);
	ASSERT_NEAR(4.f, OsX(lu), 1e-4f); // le clip a ete retrouve par son nom
	lu.SetTrigger("go");
	lu.Update(0.1f);
	ASSERT_EQUAL(b, lu.GetCurrentState()); // seuil 0,25 < 0,5 : la conjonction ne tient pas
	lu.SetFloat("seuil", 0.9f);
	lu.Update(0.1f);
	ASSERT_EQUAL(z, lu.GetCurrentState());

	// Fichier : ecrit, relu par la machine ; relu par un CLIP, c'est un clip vide.
	ASSERT_TRUE(src.SaveBinary("nkanima_test_v3.nkanim"));
	SM f;
	ASSERT_TRUE(f.LoadBinary("nkanima_test_v3.nkanim", res));
	ASSERT_EQUAL(src.GetStateCount(), f.GetStateCount());
	anim::NkAnimationClip commeClip;
	ASSERT_TRUE(commeClip.LoadBinary("nkanima_test_v3.nkanim"));
	ASSERT_EQUAL(0u, commeClip.boneCount);
	NkFile::Delete("nkanima_test_v3.nkanim");
	// Tronque : refuse.
	SM court;
	ASSERT_FALSE(court.LoadFromBytes(octets.Data(), octets.Size() - 5, res));
}

// (h9)
TEST_CASE(NKAnima, HFSM_h9_UneDefinitionDeuxPersonnages) {
	// Reference : deux machines distinctes. Essai : une seule, dont on echange
	// l'etat d'execution et les parametres a chaque pas.
	Plateforme r1, r2, partage;
	SM::NkRuntime e1, e2;
	float32 vitesse[2] = {0.f, 0.f};
	float32 vy[2] = {0.f, 0.f};
	bool auSol[2] = {true, true};
	for (int32 k = 0; k < 40; ++k) {
		vitesse[0] = (k >= 5) ? 1.f : 0.f;
		vy[0] = (k >= 25) ? 2.f : 0.f; // en montee apres le saut : il y reste
		auSol[1] = !(k >= 10 && k < 20);
		const bool saut0 = (k == 25);
		r1.sm.SetFloat("vitesse", vitesse[0]);
		r1.sm.SetFloat("vy", vy[0]);
		r1.sm.SetBool("auSol", auSol[0]);
		if (saut0) {
			r1.sm.SetTrigger("saut");
		}
		r1.sm.Update(0.05f);
		r2.sm.SetFloat("vitesse", vitesse[1]);
		r2.sm.SetBool("auSol", auSol[1]);
		r2.sm.Update(0.05f);
		for (int32 who = 0; who < 2; ++who) {
			SM &m = partage.sm;
			m.ResetParams();
			m.SetRuntime(who == 0 ? e1 : e2);
			m.SetFloat("vitesse", vitesse[who]);
			m.SetFloat("vy", vy[who]);
			m.SetBool("auSol", auSol[who]);
			if (who == 0 && saut0) {
				m.SetTrigger("saut");
			}
			m.Update(0.05f);
			(who == 0 ? e1 : e2) = m.GetRuntime();
		}
		ASSERT_EQUAL(r1.sm.GetCurrentState(), e1.current);
		ASSERT_EQUAL(r2.sm.GetCurrentState(), e2.current);
	}
	ASSERT_EQUAL(r1.saut, e1.current);	  // le premier a saute
	ASSERT_EQUAL(r2.idle, e2.current);	  // le second est retombe
	// Un index perime (modele change) repart de l'entree au lieu de lire n'importe quoi.
	SM::NkRuntime faux;
	faux.current = 999;
	partage.sm.SetRuntime(faux);
	partage.sm.Update(0.05f);
	ASSERT_EQUAL(partage.idle, partage.sm.GetCurrentState());
}
