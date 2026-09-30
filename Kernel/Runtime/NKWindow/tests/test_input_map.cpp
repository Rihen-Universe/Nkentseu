// =============================================================================
// test_input_map.cpp
//
// NkInputMap, le systeme d'entree configurable du noyau, eprouve SANS fenetre
// ni materiel : les evenements sont construits a la main, les manettes sont
// une manette de banc derriere un VRAI NkGamepadSystem.
//
// ⚠️ ICI ET PAS DANS NKEvent/tests, comme test_bindings_text.cpp : NkInputMap lit
//    NkGamepadSystem, dont l'objet tire NkEventSystem, dont PumpOS vit dans
//    NKWindow. NKEvent_Tests ne lie pas NKWindow ; NKWindow_Tests, si.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (m1)  types : bouton 0/1 ; axe 1D composite Q/D -> -1, 0 (les deux), +1 ;
//         axe 2D composite ZQSD en diagonale -> longueur 1 (pas 1,414)
//   (m2)  stick 2D, zone morte radiale 0,2 : (0,1 ; 0,1) -> 0 ; (0,6 ; 0) -> 0,5 ;
//         (0,25 ; 0,05) garde un Y non nul, que l'axiale annule
//   (m3)  modificateurs : echelle 2 bornee a 1 ; inversion ; permutation (une
//         touche vers le Y d'un axe 2D) ; courbe 2 (0,5 -> 0,25) ; lissage de
//         constante t : apres une image de t, 1 - e^-1 (0,632)
//   (m4)  declencheurs : presse (une image), relache (une image au lacher),
//         maintenu:0,5 (rien a 0,4 s, oui a 0,5 s), tape:0,25 (appui court ->
//         une impulsion ; appui long -> rien), double:0,3
//   (m5)  accord Ctrl+S -> Enregistrer, et S seul -> Reculer ; Ctrl+S ne fait
//         PAS reculer (la touche seule est bloquee par l'accord)
//   (m6)  contextes : Menu (10, consomme) prend Haut ; Jeu (0) ne le voit plus ;
//         Menu eteint -> Jeu le retrouve ; (m6b) un contexte qui TRAVERSE ne
//         reserve rien
//   (m7)  deux joueurs : manette 1 -> joueur 2 seul ; le clavier refuse par le
//         joueur 2 ; NkInputPlayers : la manette 2 qui presse Start rejoint le
//         premier joueur sans manette
//   (m8)  capture : Espace devient K ; Echap annule ; une PARTIE de composite ;
//         un bouton de manette qui VIENT d'etre enfonce
//   (m9)  texte : Save -> Load dans une carte neuve -> meme texte, memes
//         valeurs ; (m9n) cinq lignes fautives refusees AVEC leur numero, les
//         bonnes appliquees
//   (m10) rappels : DEBUT une fois, EN COURS chaque image, FIN une fois ;
//         ReleaseAll donne la FIN tout de suite
//   (m11) souris : deplacement cumule sur l'image puis 0 ; molette
//   (m12) doigt : zone bouton ; joystick flottant (+60 px -> X 1, 30 px haut ->
//         Y +0,5)
//   (m13) gestes : balayage droite -> une impulsion ; pincer -> somme des
//         variations
//   (m14) une action SANS liaison posee par programme survit a Update ; liee,
//         elle est recalculee
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKEvent/NkInputMap.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstring>

using namespace nkentseu;
using math::NkVec2f;

namespace {

	constexpr float kDt = 1.f / 60.f;

	class NkManetteDeBanc final : public NkIGamepad {
		public:
			NkGamepadSnapshot etat[NK_MAX_GAMEPADS];
			bool Init() override {
				return true;
			}
			void Shutdown() override {
			}
			void Poll() override {
			}
			uint32 GetConnectedCount() const override {
				uint32 n = 0;
				for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
					n += etat[i].connected ? 1u : 0u;
				}
				return n;
			}
			const NkGamepadSnapshot &GetSnapshot(uint32 i) const override {
				return etat[i < NK_MAX_GAMEPADS ? i : 0];
			}
			void Rumble(uint32, float32, float32, float32, float32, uint32) override {
			}
			const char *GetName() const noexcept override {
				return "ManetteDeBanc";
			}
	};

	struct NkManettes {
			NkGamepadSystem sys;
			NkManetteDeBanc *banc = nullptr;
			NkManettes() {
				memory::NkAllocator &a = memory::NkGetDefaultAllocator();
				banc = a.New<NkManetteDeBanc>();
				sys.Init(memory::NkUniquePtr<NkIGamepad>(banc, memory::NkDefaultDelete<NkIGamepad>(&a)));
			}
			void Brancher(uint32 i) {
				banc->etat[i].Clear();
				banc->etat[i].connected = true;
				sys.PollGamepads();
			}
			void Bouton(uint32 i, NkGamepadButton b, bool bas) {
				banc->etat[i].buttons[static_cast<uint32>(b)] = bas;
				sys.PollGamepads();
			}
			void Stick(uint32 i, float x, float y) {
				banc->etat[i].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX)] = x;
				banc->etat[i].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LY)] = y;
				sys.PollGamepads();
			}
	};

	void Touche(NkInputMap &m, NkKey k, bool bas) {
		if (bas) {
			NkKeyPressEvent e(k);
			m.Read(e);
		} else {
			NkKeyReleaseEvent e(k);
			m.Read(e);
		}
	}

	float Longueur(const NkVec2f &v) {
		return std::sqrt(v.x * v.x + v.y * v.y);
	}

	NkInputCode K(NkKey k) {
		return NkInputCode::Key(k);
	}

} // namespace

TEST_CASE(NKEventInputMap, M1_Types) {
	NkInputMap m;
	const NkInputActionId sauter = m.DeclareAction("Sauter", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId avancer = m.DeclareAction("Avancer", NkInputValueType::NK_INPUT_AXIS1D);
	const NkInputActionId bouger = m.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
	const int32 jeu = m.AddContext("Jeu");
	m.Context(jeu)->Add(NkInputBinding::Key(sauter, NkKey::NK_SPACE));
	m.Context(jeu)->Add(NkInputBinding::Composite1D(avancer, K(NkKey::NK_D), K(NkKey::NK_A)));
	m.Context(jeu)->Add(NkInputBinding::Composite2D(bouger, K(NkKey::NK_RIGHT), K(NkKey::NK_LEFT), K(NkKey::NK_UP),
													K(NkKey::NK_DOWN)));
	Touche(m, NkKey::NK_SPACE, true);
	Touche(m, NkKey::NK_A, true);
	Touche(m, NkKey::NK_RIGHT, true);
	Touche(m, NkKey::NK_UP, true);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.WasPressed(sauter));
	ASSERT_NEAR(1.f, m.Value(sauter), 1e-6f);
	ASSERT_NEAR(-1.f, m.Value(avancer), 1e-6f);
	ASSERT_NEAR(1.f, Longueur(m.Value2D(bouger)), 1e-5f);
	Touche(m, NkKey::NK_D, true);
	m.Update(kDt, nullptr);
	ASSERT_NEAR(0.f, m.Value(avancer), 1e-6f);
	ASSERT_FALSE(m.WasPressed(sauter));
	ASSERT_TRUE(m.IsDown(sauter));
}

TEST_CASE(NKEventInputMap, M2_StickZoneMorteRadiale) {
	NkManettes pads;
	pads.Brancher(0);
	NkInputMap m;
	const NkInputActionId bouger = m.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
	const NkInputActionId axial = m.DeclareAction("Axial", NkInputValueType::NK_INPUT_AXIS2D);
	const int32 jeu = m.AddContext("Jeu", 0, false); // traverse : les deux liaisons lisent le meme stick
	m.Context(jeu)->Add(NkInputBinding::Stick(bouger).WithDeadZone(NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL, 0.2f));
	m.Context(jeu)->Add(NkInputBinding::Stick(axial).WithDeadZone(NkInputDeadZone::NK_INPUT_DEADZONE_AXIAL, 0.2f));
	pads.Stick(0, 0.1f, 0.1f);
	m.Update(kDt, &pads.sys);
	ASSERT_NEAR(0.f, Longueur(m.Value2D(bouger)), 1e-6f);
	pads.Stick(0, 0.6f, 0.f);
	m.Update(kDt, &pads.sys);
	ASSERT_NEAR(0.5f, m.Value2D(bouger).x, 1e-5f);
	pads.Stick(0, 0.25f, 0.05f);
	m.Update(kDt, &pads.sys);
	ASSERT_GREATER(m.Value2D(bouger).y, 1e-3f);
	ASSERT_NEAR(0.f, m.Value2D(axial).y, 1e-6f);
}

TEST_CASE(NKEventInputMap, M3_Modificateurs) {
	NkManettes pads;
	pads.Brancher(0);
	NkInputMap m;
	const NkInputActionId a = m.DeclareAction("A", NkInputValueType::NK_INPUT_AXIS1D);
	const NkInputActionId inv = m.DeclareAction("Inv", NkInputValueType::NK_INPUT_AXIS1D);
	const NkInputActionId bouger = m.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
	const NkInputActionId courbe = m.DeclareAction("Courbe", NkInputValueType::NK_INPUT_AXIS1D);
	const NkInputActionId lisse = m.DeclareAction("Lisse", NkInputValueType::NK_INPUT_AXIS1D);
	const int32 c = m.AddContext("Jeu", 0, false);
	m.Context(c)->Add(NkInputBinding::Key(a, NkKey::NK_E).WithScale(2.f));
	m.Context(c)->Add(NkInputBinding::Key(inv, NkKey::NK_E).Inverted(true));
	m.Context(c)->Add(NkInputBinding::Key(bouger, NkKey::NK_E).SwappedXY());
	m.Context(c)->Add(NkInputBinding::GamepadAxis(courbe, NkGamepadAxis::NK_GP_AXIS_LX).WithCurve(2.f));
	m.Context(c)->Add(NkInputBinding::Key(lisse, NkKey::NK_E).WithSmoothing(0.1f));
	Touche(m, NkKey::NK_E, true);
	pads.Stick(0, 0.5f, 0.f);
	m.Update(0.1f, &pads.sys);
	ASSERT_NEAR(1.f, m.Value(a), 1e-6f); // 2 borne a 1
	ASSERT_NEAR(-1.f, m.Value(inv), 1e-6f);
	ASSERT_NEAR(0.f, m.Value2D(bouger).x, 1e-6f);
	ASSERT_NEAR(1.f, m.Value2D(bouger).y, 1e-6f);
	ASSERT_NEAR(0.25f, m.Value(courbe), 1e-5f);
	ASSERT_NEAR(1.f - std::exp(-1.f), m.Value(lisse), 1e-4f);
}

TEST_CASE(NKEventInputMap, M4_Declencheurs) {
	NkInputMap m;
	const NkInputActionId presse = m.DeclareAction("Presse", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId relache = m.DeclareAction("Relache", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId tenu = m.DeclareAction("Tenu", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId tape = m.DeclareAction("Tape", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId dbl = m.DeclareAction("Double", NkInputValueType::NK_INPUT_BUTTON);
	const int32 c = m.AddContext("Jeu", 0, false);
	m.Context(c)->Add(NkInputBinding::Key(presse, NkKey::NK_E).WithTrigger(NkInputTriggerKind::NK_INPUT_TRIGGER_PRESSED));
	m.Context(c)->Add(NkInputBinding::Key(relache, NkKey::NK_E).WithTrigger(NkInputTriggerKind::NK_INPUT_TRIGGER_RELEASED));
	m.Context(c)->Add(NkInputBinding::Key(tenu, NkKey::NK_E).WithTrigger(NkInputTriggerKind::NK_INPUT_TRIGGER_HOLD, 0.5f));
	m.Context(c)->Add(NkInputBinding::Key(tape, NkKey::NK_E).WithTrigger(NkInputTriggerKind::NK_INPUT_TRIGGER_TAP, 0.25f));
	m.Context(c)->Add(NkInputBinding::Key(dbl, NkKey::NK_E).WithTrigger(NkInputTriggerKind::NK_INPUT_TRIGGER_DOUBLE_TAP, 0.3f));

	// Appui court (0,1 s) : presse une image, tape au lacher, pas de maintenu.
	Touche(m, NkKey::NK_E, true);
	m.Update(0.05f, nullptr);
	ASSERT_TRUE(m.IsDown(presse));
	m.Update(0.05f, nullptr);
	ASSERT_FALSE(m.IsDown(presse));
	m.Update(0.05f, nullptr);
	Touche(m, NkKey::NK_E, false);
	m.Update(0.05f, nullptr);
	ASSERT_TRUE(m.WasPressed(relache));
	ASSERT_TRUE(m.WasPressed(tape));
	ASSERT_FALSE(m.IsDown(tenu));
	m.Update(0.05f, nullptr);
	ASSERT_FALSE(m.IsDown(relache));
	// Second appui 0,1 s apres le premier : double tape.
	Touche(m, NkKey::NK_E, true);
	m.Update(0.05f, nullptr);
	ASSERT_TRUE(m.WasPressed(dbl));
	// Tenu : rien a 0,4 s, oui a 0,5 s ; et pas de tape au lacher (trop long).
	for (int i = 0; i < 8; ++i) {
		m.Update(0.05f, nullptr);
	}
	ASSERT_FALSE(m.IsDown(tenu)); // 0,40 s
	m.Update(0.05f, nullptr);
	m.Update(0.05f, nullptr);
	ASSERT_TRUE(m.IsDown(tenu)); // 0,50 s
	Touche(m, NkKey::NK_E, false);
	m.Update(0.05f, nullptr);
	ASSERT_FALSE(m.WasPressed(tape));
}

TEST_CASE(NKEventInputMap, M5_Accord) {
	NkInputMap m;
	const NkInputActionId save = m.DeclareAction("Enregistrer", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId reculer = m.DeclareAction("Reculer", NkInputValueType::NK_INPUT_BUTTON);
	const int32 c = m.AddContext("Jeu");
	m.Context(c)->Add(NkInputBinding::Key(save, NkKey::NK_S).WithChord(K(NkKey::NK_LCTRL)));
	m.Context(c)->Add(NkInputBinding::Key(reculer, NkKey::NK_S));
	Touche(m, NkKey::NK_S, true);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(reculer));
	ASSERT_FALSE(m.IsDown(save));
	Touche(m, NkKey::NK_S, false);
	m.Update(kDt, nullptr);
	Touche(m, NkKey::NK_LCTRL, true);
	Touche(m, NkKey::NK_S, true);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(save));
	ASSERT_FALSE(m.IsDown(reculer));
}

TEST_CASE(NKEventInputMap, M6_Contextes) {
	NkInputMap m;
	const NkInputActionId avancer = m.DeclareAction("Avancer", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId monter = m.DeclareAction("MenuHaut", NkInputValueType::NK_INPUT_BUTTON);
	const int32 jeu = m.AddContext("Jeu", 0);
	const int32 menu = m.AddContext("Menu", 10, true);
	m.Context(jeu)->Add(NkInputBinding::Key(avancer, NkKey::NK_UP));
	m.Context(menu)->Add(NkInputBinding::Key(monter, NkKey::NK_UP));
	Touche(m, NkKey::NK_UP, true);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(monter));
	ASSERT_FALSE(m.IsDown(avancer));
	m.SetContextEnabled("Menu", false);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(avancer));
	ASSERT_FALSE(m.IsDown(monter));
	// (m6b) un menu qui TRAVERSE ne reserve rien.
	m.SetContextEnabled("Menu", true);
	m.Context(menu)->consume = false;
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(monter));
	ASSERT_TRUE(m.IsDown(avancer));
}

TEST_CASE(NKEventInputMap, M7_Joueurs) {
	NkManettes pads;
	pads.Brancher(0);
	pads.Brancher(1);
	pads.Brancher(2);
	NkInputMap j1;
	NkInputMap j2;
	NkInputMap *cartes[2] = {&j1, &j2};
	NkInputActionId sauter[2];
	for (int i = 0; i < 2; ++i) {
		sauter[i] = cartes[i]->DeclareAction("Sauter", NkInputValueType::NK_INPUT_BUTTON);
		const int32 c = cartes[i]->AddContext("Jeu");
		cartes[i]->Context(c)->Add(NkInputBinding::GamepadButton(sauter[i], NkGamepadButton::NK_GP_SOUTH));
		cartes[i]->Context(c)->Add(NkInputBinding::Key(sauter[i], NkKey::NK_SPACE));
	}
	j1.SetGamepad(0);
	j2.SetGamepad(1);
	j2.SetAcceptKeyboardMouse(false);
	pads.Bouton(1, NkGamepadButton::NK_GP_SOUTH, true);
	j1.Update(kDt, &pads.sys);
	j2.Update(kDt, &pads.sys);
	ASSERT_FALSE(j1.IsDown(sauter[0]));
	ASSERT_TRUE(j2.IsDown(sauter[1]));
	pads.Bouton(1, NkGamepadButton::NK_GP_SOUTH, false);
	Touche(j1, NkKey::NK_SPACE, true);
	Touche(j2, NkKey::NK_SPACE, true);
	j1.Update(kDt, &pads.sys);
	j2.Update(kDt, &pads.sys);
	ASSERT_TRUE(j1.IsDown(sauter[0]));
	ASSERT_FALSE(j2.IsDown(sauter[1]));

	// NkInputPlayers : la manette 2 presse Start et rejoint le joueur sans manette.
	NkInputMap k1;
	NkInputMap k2;
	k1.SetGamepad(0);
	k2.SetGamepad(NK_INPUT_NO_GAMEPAD);
	NkInputPlayers joueurs;
	joueurs.Add(k1);
	joueurs.Add(k2);
	ASSERT_EQUAL(-1, joueurs.Update(&pads.sys, NkGamepadButton::NK_GP_START));
	pads.Bouton(2, NkGamepadButton::NK_GP_START, true);
	ASSERT_EQUAL(1, joueurs.Update(&pads.sys, NkGamepadButton::NK_GP_START));
	ASSERT_EQUAL(2, k2.GetGamepad());
	ASSERT_EQUAL(1, joueurs.PlayerForGamepad(2));
}

TEST_CASE(NKEventInputMap, M8_Capture) {
	NkManettes pads;
	pads.Brancher(0);
	NkInputMap m;
	const NkInputActionId sauter = m.DeclareAction("Sauter", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId bouger = m.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
	const int32 c = m.AddContext("Jeu");
	const int32 b0 = m.Context(c)->Add(NkInputBinding::Key(sauter, NkKey::NK_SPACE));
	const int32 b1 = m.Context(c)->Add(NkInputBinding::Composite2D(bouger, K(NkKey::NK_D), K(NkKey::NK_A),
																	K(NkKey::NK_W), K(NkKey::NK_S)));
	ASSERT_TRUE(m.BeginRebind(c, b0));
	Touche(m, NkKey::NK_K, true); // capturee : ne presse rien
	Touche(m, NkKey::NK_K, false);
	ASSERT_TRUE(m.LastRebindSucceeded());
	Touche(m, NkKey::NK_K, true);
	m.Update(kDt, &pads.sys);
	ASSERT_TRUE(m.IsDown(sauter));
	Touche(m, NkKey::NK_K, false);
	// Echap annule, la liaison ne change pas.
	ASSERT_TRUE(m.BeginRebind(c, b0));
	Touche(m, NkKey::NK_ESCAPE, true);
	ASSERT_FALSE(m.IsRebinding());
	ASSERT_FALSE(m.LastRebindSucceeded());
	ASSERT_TRUE(m.Context(c)->bindings[static_cast<uint32>(b0)].source.code == K(NkKey::NK_K));
	// Une partie de composite : « haut » devient Z.
	ASSERT_TRUE(m.BeginRebind(c, b1, 2));
	Touche(m, NkKey::NK_Z, true);
	ASSERT_TRUE(m.Context(c)->bindings[static_cast<uint32>(b1)].source.parts[2] == K(NkKey::NK_Z));
	// Un bouton de manette tenu AVANT la capture n'est pas pris ; un neuf, si.
	pads.Bouton(0, NkGamepadButton::NK_GP_NORTH, true);
	m.Update(kDt, &pads.sys);
	ASSERT_TRUE(m.BeginRebind(c, b0));
	m.Update(kDt, &pads.sys);
	ASSERT_TRUE(m.IsRebinding());
	pads.Bouton(0, NkGamepadButton::NK_GP_EAST, true);
	m.Update(kDt, &pads.sys);
	ASSERT_FALSE(m.IsRebinding());
	ASSERT_TRUE(m.LastRebindCode() == NkInputCode::Gamepad(NkGamepadButton::NK_GP_EAST));
	// Une liaison sans code (souris) ne se capture pas.
	const NkInputActionId regard = m.DeclareAction("Regard", NkInputValueType::NK_INPUT_AXIS2D);
	const int32 b2 = m.Context(c)->Add(NkInputBinding::MouseDelta(regard));
	ASSERT_FALSE(m.BeginRebind(c, b2));
}

TEST_CASE(NKEventInputMap, M9_Texte) {
	NkInputMap a;
	const NkInputActionId sauter = a.DeclareAction("Sauter", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId bouger = a.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
	const NkInputActionId zoom = a.DeclareAction("Zoom", NkInputValueType::NK_INPUT_AXIS1D, 0.2f);
	const int32 jeu = a.AddContext("Jeu", 0, true);
	const int32 menu = a.AddContext("Menu", 10, false);
	a.SetContextEnabled(menu, false);
	a.Context(jeu)->Add(NkInputBinding::Key(sauter, NkKey::NK_SPACE));
	a.Context(jeu)->Add(NkInputBinding::GamepadButton(sauter, NkGamepadButton::NK_GP_SOUTH)
							.WithTrigger(NkInputTriggerKind::NK_INPUT_TRIGGER_HOLD, 0.35f)
							.WithChord(NkInputCode::Gamepad(NkGamepadButton::NK_GP_LB)));
	a.Context(jeu)->Add(NkInputBinding::Composite2D(bouger, K(NkKey::NK_D), K(NkKey::NK_A), K(NkKey::NK_W),
													 K(NkKey::NK_S)));
	a.Context(jeu)->Add(NkInputBinding::Stick(bouger)
							.WithDeadZone(NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL, 0.2f, 0.9f)
							.WithCurve(1.5f)
							.WithScale(1.f, -1.f));
	a.Context(jeu)->Add(NkInputBinding::MouseDelta(bouger).WithScale(0.1f, 0.1f).Inverted(false, true).WithSmoothing(0.05f));
	a.Context(jeu)->Add(NkInputBinding::Gesture(zoom, NkInputGesture::NK_INPUT_GESTURE_PINCH));
	a.Context(jeu)->Add(NkInputBinding::Code(zoom, NkInputCode::Wheel(false)).WithScale(0.5f));
	a.Context(menu)->Add(NkInputBinding::ScreenButton(sauter, 0.7f, 0.55f, 0.3f, 0.45f));
	a.Context(menu)->Add(NkInputBinding::ScreenStick(bouger, 0.f, 0.35f, 0.45f, 0.65f, 0.08f).SwappedXY());
	const NkString texte = a.Save();

	NkInputMap b;
	const NkInputMapReport r = b.Load(texte);
	ASSERT_TRUE(r.Ok());
	ASSERT_EQUAL(3, b.ActionCount());
	ASSERT_EQUAL(2, b.ContextCount());
	ASSERT_TRUE(b.Save() == texte);
	ASSERT_FALSE(b.Context(b.FindContext("Menu"))->enabled);
	Touche(a, NkKey::NK_A, true);
	Touche(b, NkKey::NK_A, true);
	a.Update(kDt, nullptr);
	b.Update(kDt, nullptr);
	ASSERT_NEAR(a.Value2D(bouger).x, b.Value2D(b.FindAction("Bouger")).x, 1e-6f);

	// (m9n) cinq lignes fautives, trois bonnes.
	const NkString fautif = "# commentaire\n"
							"action Tirer bouton\n"
							"action Tirer axe2\n"				 // 3 : autre type
							"contexte Jeu priorite=0\n"
							"lier Voler Key:SPACE\n"			 // 5 : action inconnue
							"lier Tirer Key:BLURB\n"			 // 6 : entree inconnue
							"lier Tirer Key:SPACE quand=maintenu\n" // 7 : duree manquante
							"lier Tirer Composite:Key:A\n"		 // 8 : une seule partie
							"lier Tirer Mouse:LEFT\n";
	NkInputMap c;
	const NkInputMapReport rf = c.Load(fautif);
	ASSERT_EQUAL(5u, static_cast<uint32>(rf.errors.Size()));
	ASSERT_EQUAL(3u, rf.applied);
	ASSERT_TRUE(rf.errors[0].Find("ligne 3") != NkString::npos);
	ASSERT_TRUE(rf.errors[4].Find("ligne 8") != NkString::npos);
	ASSERT_EQUAL(1, c.Context(c.FindContext("Jeu"))->Count());
}

TEST_CASE(NKEventInputMap, M10_Rappels) {
	NkInputMap m;
	const NkInputActionId tirer = m.DeclareAction("Tirer", NkInputValueType::NK_INPUT_BUTTON);
	const int32 c = m.AddContext("Jeu");
	m.Context(c)->Add(NkInputBinding::Key(tirer, NkKey::NK_F));
	int debut = 0;
	int cours = 0;
	int fin = 0;
	m.OnAction(tirer, NkInputPhase::NK_INPUT_STARTED, [&](NkInputActionId, NkInputPhase, const NkInputActionState &) { ++debut; });
	const uint32 jeton =
		m.OnAction(tirer, NkInputPhase::NK_INPUT_ONGOING, [&](NkInputActionId, NkInputPhase, const NkInputActionState &) { ++cours; });
	m.OnAction(tirer, NkInputPhase::NK_INPUT_COMPLETED, [&](NkInputActionId, NkInputPhase, const NkInputActionState &) { ++fin; });
	Touche(m, NkKey::NK_F, true);
	m.Update(kDt, nullptr);
	m.Update(kDt, nullptr);
	m.Update(kDt, nullptr);
	Touche(m, NkKey::NK_F, false);
	m.Update(kDt, nullptr);
	ASSERT_EQUAL(1, debut);
	ASSERT_EQUAL(3, cours);
	ASSERT_EQUAL(1, fin);
	// ReleaseAll : la fin arrive TOUT DE SUITE.
	m.RemoveCallback(jeton);
	Touche(m, NkKey::NK_F, true);
	m.Update(kDt, nullptr);
	m.ReleaseAll();
	ASSERT_EQUAL(2, fin);
	ASSERT_TRUE(m.WasReleased(tirer));
	ASSERT_FALSE(m.IsKeyHeld(NkKey::NK_F));
	m.Update(kDt, nullptr);
	ASSERT_FALSE(m.IsDown(tirer));
	ASSERT_EQUAL(3, cours);
}

TEST_CASE(NKEventInputMap, M11_Souris) {
	NkInputMap m;
	const NkInputActionId regard = m.DeclareAction("Regard", NkInputValueType::NK_INPUT_AXIS2D, 0.01f);
	const NkInputActionId zoom = m.DeclareAction("Zoom", NkInputValueType::NK_INPUT_AXIS1D);
	const int32 c = m.AddContext("Jeu");
	m.Context(c)->Add(NkInputBinding::MouseDelta(regard).WithScale(0.01f, 0.01f));
	m.Context(c)->Add(NkInputBinding::Code(zoom, NkInputCode::Wheel(false)));
	NkMouseMoveEvent e1(10, 10, 0, 0, 20, 0);
	NkMouseMoveEvent e2(20, 10, 0, 0, 30, -10);
	m.Read(e1);
	m.Read(e2);
	NkMouseWheelVerticalEvent w(1.0);
	m.Read(w);
	m.Update(kDt, nullptr);
	ASSERT_NEAR(0.5f, m.Value2D(regard).x, 1e-5f);
	ASSERT_NEAR(-0.1f, m.Value2D(regard).y, 1e-5f);
	ASSERT_NEAR(1.f, m.Value(zoom), 1e-6f);
	m.Update(kDt, nullptr);
	ASSERT_NEAR(0.f, m.Value2D(regard).x, 1e-6f);
	ASSERT_NEAR(0.f, m.Value(zoom), 1e-6f);
}

TEST_CASE(NKEventInputMap, M12_Doigt) {
	NkInputMap m;
	const NkInputActionId sauter = m.DeclareAction("Sauter", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId bouger = m.DeclareAction("Bouger", NkInputValueType::NK_INPUT_AXIS2D);
	const int32 c = m.AddContext("Jeu");
	m.SetScreenSurface(0.f, 0.f, 800.f, 600.f);
	m.Context(c)->Add(NkInputBinding::ScreenButton(sauter, 0.75f, 0.5f, 0.25f, 0.5f));
	m.Context(c)->Add(NkInputBinding::ScreenStick(bouger, 0.f, 0.f, 0.5f, 1.f, 0.1f));
	NkTouchPoint p;
	p.id = 7;
	p.clientX = 700.f;
	p.clientY = 500.f;
	NkTouchBeginEvent pose(&p, 1);
	m.Read(pose);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(sauter));
	NkTouchEndEvent leve(&p, 1);
	m.Read(leve);
	NkTouchPoint q;
	q.id = 3;
	q.clientX = 200.f;
	q.clientY = 300.f;
	NkTouchBeginEvent pose2(&q, 1);
	m.Read(pose2);
	q.clientX = 260.f;
	NkTouchMoveEvent bouge(&q, 1);
	m.Read(bouge);
	m.Update(kDt, nullptr);
	ASSERT_FALSE(m.IsDown(sauter));
	ASSERT_NEAR(1.f, m.Value2D(bouger).x, 1e-5f);
	q.clientX = 200.f;
	q.clientY = 270.f;
	NkTouchMoveEvent bouge2(&q, 1);
	m.Read(bouge2);
	m.Update(kDt, nullptr);
	ASSERT_NEAR(0.5f, m.Value2D(bouger).y, 1e-5f);
}

TEST_CASE(NKEventInputMap, M13_Gestes) {
	NkInputMap m;
	const NkInputActionId page = m.DeclareAction("PageSuivante", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId zoom = m.DeclareAction("Zoom", NkInputValueType::NK_INPUT_AXIS1D, 0.01f);
	const int32 c = m.AddContext("Jeu");
	m.Context(c)->Add(NkInputBinding::Gesture(page, NkInputGesture::NK_INPUT_GESTURE_SWIPE_RIGHT));
	m.Context(c)->Add(NkInputBinding::Gesture(zoom, NkInputGesture::NK_INPUT_GESTURE_PINCH));
	NkGestureSwipeEvent sw(NkSwipeDirection::NK_SWIPE_RIGHT, 900.f);
	ASSERT_TRUE(m.Read(sw));
	NkGesturePinchEvent z1(1.1f, 0.1f);
	NkGesturePinchEvent z2(1.25f, 0.15f);
	m.Read(z1);
	m.Read(z2);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.WasPressed(page));
	ASSERT_NEAR(0.25f, m.Value(zoom), 1e-5f);
	m.Update(kDt, nullptr);
	ASSERT_FALSE(m.IsDown(page));
	NkGestureSwipeEvent gauche(NkSwipeDirection::NK_SWIPE_LEFT, 900.f);
	ASSERT_FALSE(m.Read(gauche)); // aucune liaison : pas pris
}

TEST_CASE(NKEventInputMap, M14_ValeurParProgramme) {
	NkInputMap m;
	const NkInputActionId libre = m.DeclareAction("Libre", NkInputValueType::NK_INPUT_BUTTON);
	const NkInputActionId liee = m.DeclareAction("Liee", NkInputValueType::NK_INPUT_BUTTON);
	const int32 c = m.AddContext("Jeu");
	m.Context(c)->Add(NkInputBinding::Key(liee, NkKey::NK_E));
	m.SetValue(libre, 1.f);
	m.SetValue(liee, 1.f);
	m.Update(kDt, nullptr);
	ASSERT_TRUE(m.IsDown(libre));
	ASSERT_FALSE(m.IsDown(liee));
}
