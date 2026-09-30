// =============================================================================
// Engine/Noge/tests/test_noge_entrees.cpp
// =============================================================================
// Les entrees d'un jeu Noge : NkEngineLayer porte la carte du NOYAU
// (NkInputMap, le meme systeme qu'Unkeny), la nourrit par OnEvent et l'avance
// par OnUpdate. La couche est attachee SANS fenetre (elle le permet depuis le
// 2026-09-26) ; les evenements sont construits a la main.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (n1) un texte d'entrees charge dans la couche ; Espace passe par OnEvent,
//        OnUpdate l'avance : Sauter est PRESSE, et OnEvent ne consomme pas
//        (les couches du dessous recoivent toujours l'evenement)
//   (n2) le composite ZQSD / WASD donne un axe 2D de longueur 1 en diagonale ;
//        un rappel DEBUT part une fois
//   (n3) aller-retour par un vrai fichier : SaveInputFile puis LoadInputFile
//        dans une autre couche rend le meme texte ; un fichier absent est une
//        erreur NOMMEE
//   (n4) un contexte « Menu » allume par le jeu reserve Echap et prend le pas
//        sur « Jeu »
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "Noge/Layers/NkEngineLayer.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKFileSystem/NkFile.h"

#include <cmath>
#include <cstdio>

using namespace nkentseu;

namespace {

	const char *kTexte = "action Sauter bouton\n"
						 "action Bouger axe2\n"
						 "action Pause bouton\n"
						 "action Retour bouton\n"
						 "contexte Jeu priorite=0 consomme\n"
						 "lier Sauter Key:SPACE\n"
						 "lier Sauter Gamepad:SOUTH\n"
						 "lier Bouger Composite:Key:D,Key:A,Key:W,Key:S\n"
						 "lier Bouger Stick:LEFT zmr=0.2\n"
						 "lier Pause Key:ESCAPE quand=presse\n"
						 "contexte Menu priorite=10 consomme eteint\n"
						 "lier Retour Key:ESCAPE quand=presse\n";

	void Appui(NkEngineLayer &c, NkKey k, bool bas) {
		if (bas) {
			NkKeyPressEvent e(k);
			(void)c.OnEvent(&e);
		} else {
			NkKeyReleaseEvent e(k);
			(void)c.OnEvent(&e);
		}
	}

} // namespace

TEST_CASE(NogeEntrees, N1_N2_N4_LaCoucheNourritLaCarte) {
	NkEngineLayer *couche = new NkEngineLayer();
	couche->OnAttach();
	NkInputMap &in = couche->GetInput();
	const NkInputMapReport r = in.Load(NkString(kTexte));
	ASSERT_TRUE(r.Ok());
	const NkInputActionId sauter = in.FindAction("Sauter");
	const NkInputActionId bouger = in.FindAction("Bouger");
	const NkInputActionId pause = in.FindAction("Pause");
	const NkInputActionId retour = in.FindAction("Retour");
	int debuts = 0;
	in.OnAction(bouger, NkInputPhase::NK_INPUT_STARTED, [&](NkInputActionId, NkInputPhase, const NkInputActionState &) {
		++debuts;
	});

	// (n1)
	NkKeyPressEvent espace(NkKey::NK_SPACE);
	ASSERT_FALSE(couche->OnEvent(&espace));
	couche->OnUpdate(1.f / 60.f);
	ASSERT_TRUE(in.WasPressed(sauter));

	// (n2)
	Appui(*couche, NkKey::NK_D, true);
	Appui(*couche, NkKey::NK_W, true);
	couche->OnUpdate(1.f / 60.f);
	couche->OnUpdate(1.f / 60.f);
	const math::NkVec2f v = in.Value2D(bouger);
	ASSERT_NEAR(1.f, std::sqrt(v.x * v.x + v.y * v.y), 1e-5f);
	ASSERT_EQUAL(1, debuts);

	// (n4) le jeu ouvre son menu : Echap est au menu, plus a la pause.
	in.SetContextEnabled("Menu", true);
	Appui(*couche, NkKey::NK_ESCAPE, true);
	couche->OnUpdate(1.f / 60.f);
	ASSERT_TRUE(in.WasPressed(retour));
	ASSERT_FALSE(in.IsDown(pause));

	couche->OnDetach();
	delete couche;
}

TEST_CASE(NogeEntrees, N3_FichierAllerRetour) {
	NkEngineLayer *a = new NkEngineLayer();
	a->OnAttach();
	ASSERT_TRUE(a->GetInput().Load(NkString(kTexte)).Ok());
	const char *chemin = "noge_entrees_banc.nkinput";
	ASSERT_TRUE(a->SaveInputFile(chemin));
	const NkString ecrit = a->GetInput().Save();
	a->OnDetach();
	delete a;

	NkEngineLayer *b = new NkEngineLayer();
	b->OnAttach();
	const NkInputMapReport r = b->LoadInputFile(chemin);
	ASSERT_TRUE(r.Ok());
	ASSERT_TRUE(b->GetInput().Save() == ecrit);
	const NkInputMapReport absent = b->LoadInputFile("ce_fichier_n_existe_pas.nkinput");
	ASSERT_FALSE(absent.Ok());
	ASSERT_TRUE(absent.errors[0].Find("absent") != NkString::npos);
	b->OnDetach();
	delete b;
	std::remove(chemin);
}
