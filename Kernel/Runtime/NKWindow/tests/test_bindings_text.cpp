// =============================================================================
// test_bindings_text.cpp
//
// Les commandes decrites par du TEXTE, et non par un fichier : NKEvent ne lit
// aucun fichier et ne doit pas commencer. L'application apporte le contenu,
// d'ou elle veut, et convertit son propre format si elle en a un.
//
// Ce que ces tests fixent :
//   - le format « action/axis nom code [option] » est lu correctement,
//   - une ligne fautive est REFUSEE et dit son numero, jamais avalee,
//   - une action ou un axe non declare AVANT le chargement est refuse : c'est
//     le code du jeu qui decide ce qui existe, le fichier ne decide que des
//     touches,
//   - l'aller-retour ecriture puis relecture ne perd rien,
//   - Clear() vide tout, pour changer de scene.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKEvent/NkEventDispatcher.h"

using namespace nkentseu;

namespace {

	// Deux gestionnaires prets, avec leurs noms deja crees.
	struct Bac {
			NkActionManager actions;
			NkAxisManager axes;
			int sauts = 0;
			float horizontal = 0.f;

			Bac() {
				actions.CreateAction("Sauter",
									 [this](const NkString &, const NkInputCode &, bool presse, bool) {
										 if (presse)
											 ++sauts;
									 });
				// `=` et non `+=` : un axe a UNE valeur, le rappel part une fois.
				axes.CreateAxis("Horizontal",
								[this](const NkString &, const NkInputCode &, float v) { horizontal = v; });
			}
	};

} // namespace

TEST_CASE(NKEventBindings, ChargementSimple) {
	Bac bac;
	const NkString texte = "# des commandes\n"
						   "action Sauter Key:SPACE once\n"
						   "action Sauter Gamepad:SOUTH once\n"
						   "\n"
						   "axis Horizontal Key:RIGHT 1\n"
						   "axis Horizontal Key:LEFT -1\n";

	const NkBindingsReport r = NkLoadBindings(texte, bac.actions, bac.axes);
	ASSERT_TRUE(r.Ok());
	ASSERT_EQUAL(4u, r.appliquees);
	ASSERT_EQUAL(2, static_cast<int>(bac.actions.GetCommandCount()));
	ASSERT_EQUAL(2, static_cast<int>(bac.axes.GetCommandCount()));

	// La commande chargee declenche vraiment l'action.
	bac.actions.TriggerAction(NkInputCode::Key(NkKey::NK_SPACE), true);
	ASSERT_EQUAL(1, bac.sauts);

	// Et celle de la manette aussi : une action ne nomme pas l'appareil.
	bac.actions.TriggerAction(NkInputCode::Gamepad(NkGamepadButton::NK_GP_SOUTH), true);
	ASSERT_EQUAL(2, bac.sauts);
}

TEST_CASE(NKEventBindings, LignesFautivesSontRefusees) {
	Bac bac;
	const NkString texte = "action Sauter Key:PATATE\n"      // touche inconnue
						   "action Sauter\n"                  // pas assez de mots
						   "axis Horizontal Key:LEFT\n"       // echelle manquante
						   "axis Horizontal Key:LEFT abc\n"   // echelle illisible
						   "danse Sauter Key:SPACE\n"         // type inconnu
						   "action Inconnue Key:SPACE\n"      // action jamais creee
						   "action Sauter Key:SPACE\n";       // celle-ci est bonne

	const NkBindingsReport r = NkLoadBindings(texte, bac.actions, bac.axes);
	ASSERT_FALSE(r.Ok());
	ASSERT_EQUAL(1u, r.appliquees);
	ASSERT_EQUAL(6, static_cast<int>(r.erreurs.Size()));

	// Une erreur doit dire OU : sans le numero de ligne, un fichier de cent
	// lignes devient une devinette.
	ASSERT_TRUE(r.erreurs[0].Find("ligne 1") != NkString::npos);
}

TEST_CASE(NKEventBindings, AllerRetourEcritureRelecture) {
	Bac premier;
	const NkString texte = "action Sauter Key:SPACE once\n"
						   "axis Horizontal Key:RIGHT 1\n"
						   "axis Horizontal Key:LEFT -1\n";
	ASSERT_TRUE(NkLoadBindings(texte, premier.actions, premier.axes).Ok());

	const NkString ecrit = NkSaveBindings(premier.actions, premier.axes);

	Bac second;
	const NkBindingsReport r = NkLoadBindings(ecrit, second.actions, second.axes);
	ASSERT_TRUE(r.Ok());
	ASSERT_EQUAL(3u, r.appliquees);
	ASSERT_EQUAL(1, static_cast<int>(second.actions.GetCommandCount()));
	ASSERT_EQUAL(2, static_cast<int>(second.axes.GetCommandCount()));
}

TEST_CASE(NKEventBindings, ClearVideTout) {
	Bac bac;
	ASSERT_TRUE(NkLoadBindings("action Sauter Key:SPACE\naxis Horizontal Key:LEFT -1\n",
							   bac.actions, bac.axes)
					.Ok());
	ASSERT_EQUAL(1, static_cast<int>(bac.actions.GetCommandCount()));
	ASSERT_EQUAL(1, static_cast<int>(bac.axes.GetCommandCount()));

	bac.actions.Clear();
	bac.axes.Clear();
	ASSERT_EQUAL(0, static_cast<int>(bac.actions.GetCommandCount()));
	ASSERT_EQUAL(0, static_cast<int>(bac.axes.GetCommandCount()));

	// Apres Clear, les noms n'existent plus : un rechargement doit les recreer.
	// C'est voulu : changer de scene, c'est repartir d'une page blanche.
	const NkBindingsReport r = NkLoadBindings("action Sauter Key:SPACE\n", bac.actions, bac.axes);
	ASSERT_FALSE(r.Ok());
}

TEST_CASE(NKEventBindings, AxeRendUneSeuleValeur) {
	Bac bac;
	ASSERT_TRUE(NkLoadBindings("axis Horizontal Key:RIGHT 1\naxis Horizontal Key:LEFT -1\n",
							   bac.actions, bac.axes)
					.Ok());

	// Fleche droite seule : l'axe vaut +1. Le rappel part UNE fois avec le
	// total, et non une fois par commande : sans quoi le zero de la gauche
	// relachee ecraserait le +1 de la droite enfoncee.
	bac.horizontal = 42.f; // temoin : doit etre remplace, pas cumule
	bac.axes.UpdateAxes([](NkInputDevice d, uint32 c) -> float {
		return (d == NkInputDevice::NK_KEYBOARD && static_cast<NkKey>(c) == NkKey::NK_RIGHT) ? 1.f
																							 : 0.f;
	});
	ASSERT_TRUE(bac.horizontal > 0.9f && bac.horizontal < 1.1f);

	// Fleche gauche seule : -1.
	bac.axes.UpdateAxes([](NkInputDevice d, uint32 c) -> float {
		return (d == NkInputDevice::NK_KEYBOARD && static_cast<NkKey>(c) == NkKey::NK_LEFT) ? 1.f
																							: 0.f;
	});
	ASSERT_TRUE(bac.horizontal < -0.9f && bac.horizontal > -1.1f);

	// Les deux ensemble s'annulent : c'est le comportement voulu d'un axe.
	bac.axes.UpdateAxes([](NkInputDevice, uint32) -> float { return 1.f; });
	ASSERT_TRUE(bac.horizontal > -0.01f && bac.horizontal < 0.01f);
}
