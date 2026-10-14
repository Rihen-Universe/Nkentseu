// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_evenements.cpp : LE BANC DES EVENEMENTS D'UN CLIP
// (Clip/NkClipEvents.h, 14/10/2026).
//
// Une marche d'une seconde, en boucle : « pied_gauche » a 0, « pied_droit » a 0,5
// (pile sur une image a 60 Hz), « souffle » a 0,8. Chaque critere est MESURE (la
// valeur est imprimee) ; sa mutation, dans le MEME binaire par NK_RT_MUTATION, doit
// le faire rougir :
//
//   un evenement pose sur une image tire une fois       clip_events_closed  (EV1)
//   le passage de la boucle ne perd rien                clip_events_no_loop (EV1, EV2)
//
//   NK_RT_MUTATION=clip_events_closed ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKAnima/Clip/NkClipEvents.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;

namespace {

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				uint32 bits = 0u;
				if (e != nullptr) {
					bits |= std::strstr(e, "clip_events_closed") ? (uint32)NK_EV_MUT_CLOSED : 0u;
					bits |= std::strstr(e, "clip_events_no_loop") ? (uint32)NK_EV_MUT_NO_LOOP : 0u;
				}
				NkClipEventSetMutations(bits);
			}
			~Mutations() {
				NkClipEventSetMutations(0u);
			}
	};

	void Marche(NkClipEventTrack &p) {
		p.Add(0.8f, NkString("souffle"));
		p.Add(0.f, NkString("pied_gauche"));
		p.Add(0.5f, NkString("pied_droit"));
	}

} // namespace

// -----------------------------------------------------------------------------
// EV1 : CHAQUE EVENEMENT, UNE FOIS PAR CYCLE. Trois secondes jouees image par image
// a 60 Hz (la premiere image avec `includeStart`) : chaque evenement tire 3 fois
// (pied_gauche 4 : a 0, 1, 2 et 3 s), dans l'ordre du temps. Pour memoire, le lecteur
// NkAnimationPlayer sur la meme marche : son marker « pied_droit » est compte et
// imprime (constat : il ne tire qu'au premier cycle).
//                                  [mutations : clip_events_closed ; clip_events_no_loop]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, EV1_Chaque_evenement_une_fois_par_cycle) {
	Mutations _m;
	NkClipEventTrack p;
	Marche(p);
	ASSERT_TRUE(p.events[0].name == NkString("pied_gauche") && p.events[2].name == NkString("souffle"));
	uint32 compte[3] = {0, 0, 0};
	NkVector<uint32> tout, out;
	for (int32 i = 0; i < 180; ++i) {
		const float32 e0 = (float32)i / 60.f, e1 = (float32)(i + 1) / 60.f;
		out.Clear();
		p.Collect(1.f, true, e0, e1, out, i == 0);
		for (usize k = 0; k < out.Size(); ++k) {
			++compte[out[k]];
			tout.PushBack(out[k]);
		}
	}
	// L'ordre du temps : gauche (0), droit (0,5), souffle (0,8), gauche (1), ...
	bool ordre = tout.Size() == 10u;
	for (usize k = 0; ordre && k < tout.Size(); ++k) {
		ordre = tout[k] == (uint32)(k % 3u);
	}
	// Le lecteur existant, pour memoire.
	NkAnimationClip clip;
	clip.duration = 1.f;
	clip.loop = true;
	clip.position.AddKey(0.f, NkVec3f{0.f, 0.f, 0.f});
	clip.position.AddKey(1.f, NkVec3f{0.f, 0.f, 1.f});
	NkAnimationPlayer lecteur;
	lecteur.SetClip(&clip);
	uint32 lecteurPas = 0;
	lecteur.AddMarker(0.5f, NkString("pied_droit"));
	lecteur.SetMarkerCallback([&lecteurPas](const NkString &) { ++lecteurPas; });
	lecteur.Play(NkPlayMode::NK_LOOP, 1.f);
	for (int32 i = 0; i < 180; ++i) {
		lecteur.Update(1.f / 60.f);
	}
	std::printf("  [EV1] 3 s a 60 Hz : pied_gauche %u (4 : a 0, 1, 2, 3 s), pied_droit %u (3), souffle %u (3), ordre du temps = %d ; "
				"pour memoire, le marker du lecteur NkAnimationPlayer a tire %u fois\n",
				compte[0], compte[1], compte[2], ordre ? 1 : 0, lecteurPas);
	ASSERT_TRUE_MSG(compte[0] == 4u && compte[1] == 3u && compte[2] == 3u, "chaque evenement tire une fois par cycle, meme pose sur une image");
	ASSERT_TRUE_MSG(ordre, "dans l'ordre du temps");
}

// -----------------------------------------------------------------------------
// EV2 : LES SAUTS DE TEMPS ET LE REBOURS. De 0,7 a 3,2 s d'un coup : souffle, puis
// les deux pieds deux fois, souffle, ..., dans l'ordre (8 evenements). A rebours de
// 1,6 a 0,4 s : pied_droit (1,5), pied_gauche (1,0), souffle (0,8), pied_droit (0,5),
// du plus tard au plus tot. Un clip qui ne boucle pas : de 0,3 a 5 s, pied_droit
// et souffle, une fois.                                  [mutation : clip_events_no_loop]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, EV2_Les_sauts_de_temps_et_le_rebours) {
	Mutations _m;
	NkClipEventTrack p;
	Marche(p);
	NkVector<uint32> saut, rebours, une;
	p.Collect(1.f, true, 0.7f, 3.2f, saut);
	p.Collect(1.f, true, 1.6f, 0.4f, rebours);
	p.Collect(1.f, false, 0.3f, 5.f, une);
	// Attendus : 0,8 1,0 1,5 1,8 2,0 2,5 2,8 3,0 -> souffle, gauche, droit, souffle, gauche, droit, souffle, gauche.
	const uint32 attenduSaut[8] = {2, 0, 1, 2, 0, 1, 2, 0};
	bool sautJuste = saut.Size() == 8u;
	for (usize k = 0; sautJuste && k < 8u; ++k) {
		sautJuste = saut[k] == attenduSaut[k];
	}
	const bool reboursJuste = rebours.Size() == 4u && rebours[0] == 1u && rebours[1] == 0u && rebours[2] == 2u && rebours[3] == 1u;
	const bool uneJuste = une.Size() == 2u && une[0] == 1u && une[1] == 2u;
	std::printf("  [EV2] 0,7 -> 3,2 s : %u evenements (8), dans l'ordre = %d ; 1,6 -> 0,4 s a rebours : %u (4), ordre = %d ; sans boucle 0,3 -> 5 s : "
				"%u (2) = %d\n",
				(uint32)saut.Size(), sautJuste ? 1 : 0, (uint32)rebours.Size(), reboursJuste ? 1 : 0, (uint32)une.Size(), uneJuste ? 1 : 0);
	ASSERT_TRUE_MSG(sautJuste, "un saut de temps traverse tous les cycles, dans l'ordre");
	ASSERT_TRUE_MSG(reboursJuste, "a rebours : du plus tard au plus tot");
	ASSERT_TRUE_MSG(uneJuste, "un clip qui ne boucle pas : ses evenements une fois");

	// Un evenement pose a la FIN du clip (t = D) : c'est le debut du cycle suivant. De
	// 0,6 a 2,6 s : milieu (0,7), fin (1,0), milieu (1,7), fin (2,0) ; 2,7 est hors.
	NkClipEventTrack q;
	q.Add(1.f, NkString("fin"));
	q.Add(0.7f, NkString("milieu")); // indice 0 (range avant « fin », indice 1)
	NkVector<uint32> f;
	q.Collect(1.f, true, 0.6f, 2.6f, f);
	const bool finJuste = f.Size() == 4u && f[0] == 0u && f[1] == 1u && f[2] == 0u && f[3] == 1u;
	std::printf("  [EV2] evenement pose a t = D : %u rencontres (4), ordre milieu / fin / milieu / fin = %d\n", (uint32)f.Size(), finJuste ? 1 : 0);
	ASSERT_TRUE_MSG(finJuste, "un evenement pose a la fin du clip tire au debut de chaque cycle, a son rang");
}
