// =============================================================================
// test_input_from_string.cpp
//
// Relire une commande depuis du texte. Sans ces conversions, une association
// touche-action ne pouvait pas venir d'un fichier : on savait ecrire
// "NK_SPACE", pas le relire. Un menu de configuration des commandes etait donc
// impossible, et les associations restaient figees dans le code.
//
// Ces tests fixent trois choses :
//   1. l'aller-retour NkInputCode -> texte -> NkInputCode ne perd rien,
//   2. un joueur peut ecrire "SPACE" ou "space" sans prefixe ni majuscules,
//   3. un texte qui ne designe rien est REFUSE, et le dit.
//
// Le troisieme point est le plus important : une configuration illisible qui
// passe en silence donne un jeu ou une touche ne repond pas, sans message.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKEvent/NkEventDispatcher.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkGamepadEvent.h"

using namespace nkentseu;

TEST_CASE(NKEventInputFromString, ToucheAvecEtSansPrefixe) {
	bool ok = false;

	const NkInputCode a = NkInputCode::FromString("Key:NK_SPACE", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(a.device == NkInputDevice::NK_KEYBOARD);
	ASSERT_TRUE(static_cast<NkKey>(a.code) == NkKey::NK_SPACE);

	// Sans prefixe d'appareil : on suppose une touche.
	const NkInputCode b = NkInputCode::FromString("SPACE", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(static_cast<NkKey>(b.code) == NkKey::NK_SPACE);

	// Casse indifferente, des deux cotes.
	const NkInputCode c = NkInputCode::FromString("key:left", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(static_cast<NkKey>(c.code) == NkKey::NK_LEFT);
}

TEST_CASE(NKEventInputFromString, SourisMolettteEtManette) {
	bool ok = false;

	const NkInputCode m = NkInputCode::FromString("Mouse:LEFT", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(m.device == NkInputDevice::NK_MOUSE);
	ASSERT_TRUE(static_cast<NkMouseButton>(m.code) == NkMouseButton::NK_MB_LEFT);

	const NkInputCode w = NkInputCode::FromString("Wheel:H", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(w.device == NkInputDevice::NK_MOUSEWHEEL);
	ASSERT_EQUAL(1u, w.code);

	const NkInputCode g = NkInputCode::FromString("Gamepad:SOUTH", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(g.device == NkInputDevice::NK_GAMEPAD);
	ASSERT_TRUE(static_cast<NkGamepadButton>(g.code) == NkGamepadButton::NK_GP_SOUTH);

	const NkInputCode ax = NkInputCode::FromString("GamepadAxis:LEFT_X", &ok);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(ax.device == NkInputDevice::NK_GAMEPAD_AXIS);
	ASSERT_TRUE(static_cast<NkGamepadAxis>(ax.code) == NkGamepadAxis::NK_GP_AXIS_LX);
}

TEST_CASE(NKEventInputFromString, AllerRetourSansPerte) {
	const NkInputCode codes[] = {
		NkInputCode::Key(NkKey::NK_ESCAPE),
		NkInputCode::Key(NkKey::NK_NUM1),
		NkInputCode::Mouse(NkMouseButton::NK_MB_RIGHT),
		NkInputCode::Gamepad(NkGamepadButton::NK_GP_NORTH),
		NkInputCode::GamepadAxis(NkGamepadAxis::NK_GP_AXIS_RY),
	};

	for (const NkInputCode &attendu : codes) {
		bool ok = false;
		const NkInputCode relu = NkInputCode::FromString(attendu.ToString(), &ok);
		ASSERT_TRUE(ok);
		ASSERT_TRUE(relu == attendu);
	}
}

TEST_CASE(NKEventInputFromString, TexteInvalideEstRefuse) {
	bool ok = true;

	// Une touche qui n'existe pas.
	NkInputCode::FromString("Key:PATATE", &ok);
	ASSERT_FALSE(ok);

	// Un appareil qui n'existe pas.
	ok = true;
	NkInputCode::FromString("Clavier:SPACE", &ok);
	ASSERT_FALSE(ok);

	// Vide.
	ok = true;
	NkInputCode::FromString("", &ok);
	ASSERT_FALSE(ok);

	// Le pointeur nul ne doit pas faire tomber le programme.
	ok = true;
	NkInputCode::FromString(static_cast<const char *>(nullptr), &ok);
	ASSERT_FALSE(ok);
}

TEST_CASE(NKEventInputFromString, ConversionsDirectes) {
	ASSERT_TRUE(NkKeyFromString("NK_A") == NkKey::NK_A);
	ASSERT_TRUE(NkKeyFromString("a") == NkKey::NK_A);
	ASSERT_TRUE(NkKeyFromString("inexistant") == NkKey::NK_UNKNOWN);

	ASSERT_TRUE(NkMouseButtonFromString("MIDDLE") == NkMouseButton::NK_MB_MIDDLE);
	ASSERT_TRUE(NkMouseButtonFromString("rien") == NkMouseButton::NK_MB_UNKNOWN);

	ASSERT_TRUE(NkGamepadButtonFromString("DPAD_UP") == NkGamepadButton::NK_GP_DPAD_UP);
	ASSERT_TRUE(NkGamepadButtonFromString("dpad_up") == NkGamepadButton::NK_GP_DPAD_UP);

	// Le prefixe est ignore des deux cotes : "NK_GP_SOUTH" repond a "SOUTH".
	ASSERT_TRUE(NkInputNameEquals("NK_GP_SOUTH", "SOUTH"));
	ASSERT_TRUE(NkInputNameEquals("NK_SPACE", "space"));
	ASSERT_FALSE(NkInputNameEquals("NK_SPACE", "SPACEBAR"));
}
