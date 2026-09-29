// =============================================================================
// test_gesture_recognizer.cpp
//
// Le reconnaisseur de gestes, nourri de suites de points SYNTHETIQUES, avec
// des instants choisis : aucun doigt, aucune horloge, aucune plateforme. Le
// meme banc tient donc partout ou NKEvent compile.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (g1)  tape : appui 100 ms sans bouger -> UNE tape, compte 1, un doigt, au
//         point d'appui ; ni pan, ni balayage, ni appui long
//   (g2)  double tape : la seconde commence 150 ms apres la premiere -> compte
//         2 ; (g2n) 400 ms apres, ou a 100 px : compte 1
//   (g3)  appui long : rien a 499 ms, UN appui long a 500 ms (duree 500), et
//         pas de tape au relachement ; (g3n) un doigt qui glisse de 20 px a
//         200 ms n'en donne pas
//   (g4)  pan : rien sous la tolerance (5 px) ; au-dela, le premier pan porte
//         tout le trajet (20 px) ; la somme des deltas = deplacement total ;
//         lent : ni tape ni balayage au relachement
//   (g5)  balayage : 150 px en 110 ms -> DROITE, vitesse ~1364 px/s ; vers le
//         haut, y diminue -> HAUT ; (g5n) les memes 150 px en 600 ms : aucun
//   (g6)  pincer : 100 -> 120 px d'ecart = echelle 1,2 au centre (160, 100),
//         puis 150 px = 1,5 (delta 0,3) ; (g6n) 100 -> 102 px : aucun
//   (g7)  rotation : la paire tourne de 10 deg (sens horaire a l'ecran) ->
//         angle 10, sens horaire, et PAS de pincement (distance gardee)
//   (g8)  annulation : un glisser annule ne donne ni balayage ni tape, et le
//         reconnaisseur revient a zero contact
//   (g9)  32 doigts poses et leves ensemble : une tape a 32 doigts ; un 33e
//         doigt est ignore sans rien casser
//   (g10) un doigt qui reste pose pendant qu'un second tape ne fait PAS de
//         tape (la session dure tant qu'un doigt reste)
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKEvent/NkGestureRecognizer.h"
#include "NKEvent/NkTouchEvent.h"

#include <cmath>

using namespace nkentseu;

namespace {

	/// Ce que le reconnaisseur a emis, en clair.
	struct Geste {
			NkEventType::Value type = NkEventType::NK_NONE;
			float x = 0.f;
			float y = 0.f;
			float valeur = 0.f;	 ///< echelle, angle, vitesse, duree selon le geste
			float delta = 0.f;	 ///< delta d'echelle / d'angle
			uint32 compte = 0;	 ///< tapCount
			uint32 doigts = 0;
			NkSwipeDirection dir = NkSwipeDirection::NK_SWIPE_NONE;
			bool horaire = false;
	};

	struct Banc {
			NkGestureRecognizer r;
			Geste g[256];
			uint32 n = 0;

			Banc() {
				r.SetSink([this](NkEvent &e) {
					if (n >= 256) {
						return;
					}
					Geste &s = g[n++];
					s.type = e.GetType();
					if (auto *t = e.As<NkGestureTapEvent>()) {
						s.x = t->GetX();
						s.y = t->GetY();
						s.compte = t->GetTapCount();
						s.doigts = t->GetNumFingers();
					} else if (auto *l = e.As<NkGestureLongPressEvent>()) {
						s.x = l->GetX();
						s.y = l->GetY();
						s.valeur = l->GetDurationMs();
						s.doigts = l->GetNumFingers();
					} else if (auto *p = e.As<NkGesturePanEvent>()) {
						s.x = p->GetDeltaX();
						s.y = p->GetDeltaY();
						s.doigts = p->GetNumFingers();
					} else if (auto *w = e.As<NkGestureSwipeEvent>()) {
						s.dir = w->GetDirection();
						s.valeur = w->GetSpeed();
						s.doigts = w->GetNumFingers();
					} else if (auto *z = e.As<NkGesturePinchEvent>()) {
						s.valeur = z->GetScale();
						s.delta = z->GetScaleDelta();
						s.x = z->GetCenterX();
						s.y = z->GetCenterY();
					} else if (auto *o = e.As<NkGestureRotateEvent>()) {
						s.valeur = o->GetAngle();
						s.delta = o->GetAngleDelta();
						s.horaire = o->IsClockwise();
					}
				});
			}

			uint32 Compter(NkEventType::Value t) const {
				uint32 c = 0;
				for (uint32 i = 0; i < n; ++i) {
					if (g[i].type == t) {
						++c;
					}
				}
				return c;
			}

			const Geste *Dernier(NkEventType::Value t) const {
				for (uint32 i = n; i > 0; --i) {
					if (g[i - 1].type == t) {
						return &g[i - 1];
					}
				}
				return nullptr;
			}

			static NkTouchPoint Pt(uint64 id, float x, float y) {
				NkTouchPoint p;
				p.id = id;
				p.clientX = x;
				p.clientY = y;
				return p;
			}

			void Poser(uint64 id, float x, float y, double t) {
				const NkTouchPoint p = Pt(id, x, y);
				NkTouchBeginEvent e(&p, 1);
				r.OnTouchEvent(e, t);
			}

			void Bouger(uint64 id, float x, float y, double t) {
				const NkTouchPoint p = Pt(id, x, y);
				NkTouchMoveEvent e(&p, 1);
				r.OnTouchEvent(e, t);
			}

			void Bouger2(uint64 ia, float xa, float ya, uint64 ib, float xb, float yb, double t) {
				const NkTouchPoint p[2] = {Pt(ia, xa, ya), Pt(ib, xb, yb)};
				NkTouchMoveEvent e(p, 2);
				r.OnTouchEvent(e, t);
			}

			void Lever(uint64 id, float x, float y, double t) {
				const NkTouchPoint p = Pt(id, x, y);
				NkTouchEndEvent e(&p, 1);
				r.OnTouchEvent(e, t);
			}

			void Annuler(double t) {
				const NkTouchPoint p = Pt(1, 0.f, 0.f);
				NkTouchCancelEvent e(&p, 1);
				r.OnTouchEvent(e, t);
			}
	};

	constexpr NkEventType::Value TAP = NkEventType::NK_GESTURE_TAP;
	constexpr NkEventType::Value LONG = NkEventType::NK_GESTURE_LONG_PRESS;
	constexpr NkEventType::Value PAN = NkEventType::NK_GESTURE_PAN;
	constexpr NkEventType::Value SWIPE = NkEventType::NK_GESTURE_SWIPE;
	constexpr NkEventType::Value PINCH = NkEventType::NK_GESTURE_PINCH;
	constexpr NkEventType::Value ROTATE = NkEventType::NK_GESTURE_ROTATE;

} // namespace

TEST_CASE(NKEventGestes, G1_TapeSimple) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Lever(1, 100.f, 100.f, 100.0);
	ASSERT_EQUAL(1u, b.Compter(TAP));
	const Geste *t = b.Dernier(TAP);
	ASSERT_NOT_NULL(t);
	ASSERT_EQUAL(1u, t->compte);
	ASSERT_EQUAL(1u, t->doigts);
	ASSERT_NEAR(100.f, t->x, 1e-4f);
	ASSERT_NEAR(100.f, t->y, 1e-4f);
	ASSERT_EQUAL(0u, b.Compter(PAN));
	ASSERT_EQUAL(0u, b.Compter(SWIPE));
	ASSERT_EQUAL(0u, b.Compter(LONG));
	ASSERT_EQUAL(0u, b.r.GetActiveContactCount());
}

TEST_CASE(NKEventGestes, G2_DoubleTape) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Lever(1, 100.f, 100.f, 100.0);
	b.Poser(2, 104.f, 98.f, 250.0);
	b.Lever(2, 104.f, 98.f, 320.0);
	ASSERT_EQUAL(2u, b.Compter(TAP));
	ASSERT_EQUAL(2u, b.Dernier(TAP)->compte);

	// (g2n) trop tard
	Banc c;
	c.Poser(1, 100.f, 100.f, 0.0);
	c.Lever(1, 100.f, 100.f, 100.0);
	c.Poser(2, 100.f, 100.f, 500.0);
	c.Lever(2, 100.f, 100.f, 560.0);
	ASSERT_EQUAL(1u, c.Dernier(TAP)->compte);

	// (g2n) trop loin
	Banc d;
	d.Poser(1, 100.f, 100.f, 0.0);
	d.Lever(1, 100.f, 100.f, 100.0);
	d.Poser(2, 200.f, 100.f, 200.0);
	d.Lever(2, 200.f, 100.f, 260.0);
	ASSERT_EQUAL(1u, d.Dernier(TAP)->compte);
}

TEST_CASE(NKEventGestes, G3_AppuiLong) {
	Banc b;
	b.Poser(1, 50.f, 60.f, 0.0);
	ASSERT_EQUAL(0u, b.r.Update(499.0));
	ASSERT_EQUAL(1u, b.r.Update(500.0));
	ASSERT_EQUAL(0u, b.r.Update(900.0)); // une seule fois
	const Geste *l = b.Dernier(LONG);
	ASSERT_NOT_NULL(l);
	ASSERT_NEAR(500.f, l->valeur, 1e-3f);
	ASSERT_NEAR(50.f, l->x, 1e-4f);
	b.Lever(1, 50.f, 60.f, 1000.0);
	ASSERT_EQUAL(0u, b.Compter(TAP));

	// (g3n) le doigt a glisse : ce n'est plus un appui long
	Banc c;
	c.Poser(1, 50.f, 60.f, 0.0);
	c.Bouger(1, 70.f, 60.f, 200.0);
	c.r.Update(600.0);
	ASSERT_EQUAL(0u, c.Compter(LONG));
}

TEST_CASE(NKEventGestes, G4_Glisser) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Bouger(1, 105.f, 100.f, 16.0);
	ASSERT_EQUAL(0u, b.Compter(PAN));
	b.Bouger(1, 120.f, 100.f, 32.0);
	ASSERT_EQUAL(1u, b.Compter(PAN));
	ASSERT_NEAR(20.f, b.Dernier(PAN)->x, 1e-4f);
	b.Bouger(1, 130.f, 110.f, 48.0);
	b.Bouger(1, 160.f, 140.f, 200.0);
	float sx = 0.f;
	float sy = 0.f;
	for (uint32 i = 0; i < b.n; ++i) {
		if (b.g[i].type == PAN) {
			sx += b.g[i].x;
			sy += b.g[i].y;
		}
	}
	ASSERT_NEAR(60.f, sx, 1e-3f);
	ASSERT_NEAR(40.f, sy, 1e-3f);
	b.Lever(1, 160.f, 140.f, 900.0);
	ASSERT_EQUAL(0u, b.Compter(TAP));
	ASSERT_EQUAL(0u, b.Compter(SWIPE));
}

TEST_CASE(NKEventGestes, G5_Balayage) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Bouger(1, 180.f, 102.f, 50.0);
	b.Bouger(1, 250.f, 105.f, 100.0);
	b.Lever(1, 250.f, 105.f, 110.0);
	ASSERT_EQUAL(1u, b.Compter(SWIPE));
	const Geste *s = b.Dernier(SWIPE);
	ASSERT_TRUE(s->dir == NkSwipeDirection::NK_SWIPE_RIGHT);
	const float attendu = std::sqrt(150.f * 150.f + 5.f * 5.f) * 1000.f / 110.f;
	ASSERT_NEAR(attendu, s->valeur, 1.f);
	ASSERT_EQUAL(0u, b.Compter(TAP));

	// vers le haut : y diminue a l'ecran
	Banc h;
	h.Poser(1, 100.f, 300.f, 0.0);
	h.Bouger(1, 102.f, 150.f, 80.0);
	h.Lever(1, 102.f, 150.f, 90.0);
	ASSERT_TRUE(h.Dernier(SWIPE) != nullptr && h.Dernier(SWIPE)->dir == NkSwipeDirection::NK_SWIPE_UP);

	// (g5n) le meme trajet, lentement : un pan, pas un balayage
	Banc c;
	c.Poser(1, 100.f, 100.f, 0.0);
	c.Bouger(1, 180.f, 102.f, 300.0);
	c.Bouger(1, 250.f, 105.f, 590.0);
	c.Lever(1, 250.f, 105.f, 600.0);
	ASSERT_EQUAL(0u, c.Compter(SWIPE));
	ASSERT_TRUE(c.Compter(PAN) > 0u);
}

TEST_CASE(NKEventGestes, G6_Pincer) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Poser(2, 200.f, 100.f, 5.0);
	b.Bouger2(1, 100.f, 100.f, 2, 220.f, 100.f, 20.0);
	ASSERT_EQUAL(1u, b.Compter(PINCH));
	const Geste *z = b.Dernier(PINCH);
	ASSERT_NEAR(1.2f, z->valeur, 1e-4f);
	ASSERT_NEAR(160.f, z->x, 1e-4f);
	ASSERT_NEAR(100.f, z->y, 1e-4f);
	b.Bouger2(1, 100.f, 100.f, 2, 250.f, 100.f, 40.0);
	z = b.Dernier(PINCH);
	ASSERT_NEAR(1.5f, z->valeur, 1e-4f);
	ASSERT_NEAR(0.3f, z->delta, 1e-4f);
	ASSERT_EQUAL(0u, b.Compter(ROTATE));
	ASSERT_TRUE(b.r.IsPinching());

	// (g6n) sous le seuil : rien
	Banc c;
	c.Poser(1, 100.f, 100.f, 0.0);
	c.Poser(2, 200.f, 100.f, 5.0);
	c.Bouger2(1, 100.f, 100.f, 2, 202.f, 100.f, 20.0);
	ASSERT_EQUAL(0u, c.Compter(PINCH));
}

TEST_CASE(NKEventGestes, G7_Rotation) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Poser(2, 200.f, 100.f, 5.0);
	const float a = 10.f * 3.14159265f / 180.f;
	b.Bouger2(1, 100.f, 100.f, 2, 100.f + 100.f * std::cos(a), 100.f + 100.f * std::sin(a), 20.0);
	ASSERT_EQUAL(1u, b.Compter(ROTATE));
	const Geste *o = b.Dernier(ROTATE);
	ASSERT_NEAR(10.f, o->valeur, 1e-2f);
	ASSERT_TRUE(o->horaire);
	ASSERT_EQUAL(0u, b.Compter(PINCH));
}

TEST_CASE(NKEventGestes, G8_Annulation) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Bouger(1, 250.f, 100.f, 50.0);
	b.Annuler(60.0);
	ASSERT_EQUAL(0u, b.Compter(SWIPE));
	ASSERT_EQUAL(0u, b.Compter(TAP));
	ASSERT_EQUAL(0u, b.r.GetActiveContactCount());
	// Apres l'annulation, une tape toute neuve compte 1.
	b.Poser(3, 100.f, 100.f, 200.0);
	b.Lever(3, 100.f, 100.f, 260.0);
	ASSERT_EQUAL(1u, b.Compter(TAP));
	ASSERT_EQUAL(1u, b.Dernier(TAP)->compte);
}

TEST_CASE(NKEventGestes, G9_TrenteDeuxDoigts) {
	Banc b;
	NkTouchPoint p[33];
	for (uint32 i = 0; i < 33; ++i) {
		p[i] = Banc::Pt(100 + i, 10.f * static_cast<float>(i), 50.f);
	}
	NkTouchBeginEvent pose(p, 33); // l'evenement lui-meme borne a 32
	b.r.OnTouchEvent(pose, 0.0);
	ASSERT_EQUAL(32u, b.r.GetActiveContactCount());
	// Un 33e doigt arrive seul : ignore, sans rien casser.
	NkTouchBeginEvent trop(&p[32], 1);
	b.r.OnTouchEvent(trop, 5.0);
	ASSERT_EQUAL(32u, b.r.GetActiveContactCount());
	NkTouchEndEvent leve(p, 32);
	b.r.OnTouchEvent(leve, 80.0);
	ASSERT_EQUAL(0u, b.r.GetActiveContactCount());
	ASSERT_EQUAL(1u, b.Compter(TAP));
	ASSERT_EQUAL(32u, b.Dernier(TAP)->doigts);
}

TEST_CASE(NKEventGestes, G10_UnDoigtResteNePasTaper) {
	Banc b;
	b.Poser(1, 100.f, 100.f, 0.0);
	b.Poser(2, 300.f, 100.f, 20.0);
	b.Lever(2, 300.f, 100.f, 80.0);
	ASSERT_EQUAL(0u, b.Compter(TAP));
	ASSERT_EQUAL(1u, b.r.GetActiveContactCount());
}
