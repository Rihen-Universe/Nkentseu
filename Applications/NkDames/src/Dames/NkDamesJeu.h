// =============================================================================
// NkDamesJeu.h — la classe application
//
// A QUOI SERT CE FICHIER
//   Il fait le lien entre les trois autres : il tient la partie (NkDamesRegles),
//   il calcule la geometrie et appelle le dessin (NkDamesEcran), et il recoit
//   les entrees. C'est le SEUL fichier du jeu qui a le droit de modifier l'etat.
//
// CE QU'IL NE FAIT PAS, ET C'EST TOUT L'INTERET
//   Ni fenetre, ni boucle, ni pointeur souris/doigt, ni zone sure, ni rotation,
//   ni cycle de vie mobile. Tout cela vient de NkCanvasGuiApp.
//
// LES MODES DE JEU
//   Chaque siege a un CONTROLEUR : humain ou IA. Trois modes en decoulent, sans
//   qu'aucun ne soit un cas particulier du code :
//     humain / IA      le defaut
//     humain / humain  a deux sur le meme ecran   (--mode=duo)
//     IA / IA          simulation                 (--mode=ia)
//   Les deux boutons de siege les basculent en cours de partie.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une regle du jeu   -> NkDamesRegles
//   - un element visuel  -> NkDamesEcran
//   - un cas de banc     -> NkDamesBanc
//   - un enchainement    -> ici
// =============================================================================
#pragma once

#include "Dames/NkDamesEcran.h"
#include "Dames/NkDamesRegles.h"
#include "NKCanvas/App/NkCanvasGuiApp.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

			class NkDamesJeu : public renderer::NkCanvasGuiApp {
				public:
					NkDamesJeu();

				protected:
					NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
					bool OnGuiInit() override;
					void OnLayout(const renderer::NkLayoutInfo &info) override;
					bool OnPointer(const NkPointer &p) override;
					void OnTick(float32 deltaTime) override;
					void OnDraw(nkgui::NkGuiDrawList &dl) override;

				private:
					void Rejouer();
					void ChoisirMode(NkMode mode);
					void VerifierFin();
					void AppliquerMode(const NkString &mode);
					void JouerCoupIA();
					/// Arme l'animation d'un coup DEJA joue. La partie est a jour ;
					/// ceci ne fait que retarder ce que l'oeil voit.
					/// `piece` ET `prises` sont lues AVANT que le coup soit joue :
					/// apres, la case de depart est vide et les pieces prises ont
					/// disparu du damier.
					void ArmerAnimation(const NkDamesCoup &coup, NkDamesPiece piece,
										const NkDamesPiece *prises);
					NkDamesVue Vue() const;

					int32 TraitIndex() const noexcept {
						return mPartie.Trait() == NkDamesCamp::NK_BLANC ? 0 : 1;
					}
					bool EstHumain(int32 siege) const noexcept {
						return mControleur[siege] == NkControleur::NK_HUMAIN;
					}

					NkDamesPartie mPartie;
					NkVector<NkDamesCoup> mCoupsProposes;
					NkDamesGeometrie mGeo;
					NkEcran mEcran = NkEcran::NK_MENU; ///< on commence par CHOISIR
					NkDamesAnim mAnim;

					/// Par defaut : vous les blancs, l'IA les noirs.
					NkControleur mControleur[2] = {NkControleur::NK_HUMAIN, NkControleur::NK_IA};

					int32 mSelR = -1, mSelC = -1;
					bool mFinie = false;
					NkDamesCamp mGagnant = NkDamesCamp::NK_BLANC;
					float32 mAttenteIA = 0.f;
					uint32 mGraine = 20260901u;
			};

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
