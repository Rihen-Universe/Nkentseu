// =============================================================================
// NkLudoJeu.h — la classe application
//
// A QUOI SERT CE FICHIER
//   Il tient la partie (NkLudoRegles), calcule la geometrie et appelle le dessin
//   (NkLudoEcran), recoit les entrees. Le SEUL fichier qui modifie l'etat.
//
// LE TOUR DE LUDO N'EST PAS UN TOUR D'ECHECS
//   Il se joue en DEUX temps : on lance le de, PUIS on choisit un pion. Entre
//   les deux, l'etat `mDeLance` dit ou l'on en est — et le bouton change de
//   libelle en consequence. Confondre les deux temps est le defaut classique :
//   on relance le de en croyant jouer.
//
// LES MODES : quatre sieges, chacun humain ou IA.
//   1 joueur + 3 IA    le defaut          (--mode=solo)
//   2 joueurs + 2 IA                      (--mode=duo)
//   4 joueurs                             (--mode=quatre)
//   4 IA                simulation        (--mode=ia)
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une regle du jeu   -> NkLudoRegles
//   - un element visuel  -> NkLudoEcran
//   - un cas de banc     -> NkLudoBanc
//   - un enchainement    -> ici
// =============================================================================
#pragma once

#include "Ludo/NkLudoEcran.h"
#include "Ludo/NkLudoRegles.h"
#include "NKCanvas/App/NkCanvasGuiApp.h"

namespace nkentseu {
	namespace jeux {
		namespace ludo {

			class NkLudoJeu : public renderer::NkCanvasGuiApp {
				public:
					NkLudoJeu();

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
					void AppliquerMode(const NkString &mode);
					void LancerLeDe();
					void PasserLaMain();
					void TerminerCoup();
					/// Arme l'animation d'un coup DEJA joue, le long de son chemin.
					void ArmerAnimation(int32 joueur, const NkLudoCoup &coup, int32 avancementAvant);
					NkLudoVue Vue() const;

					bool EstHumain(int32 siege) const noexcept {
						return mControleur[siege] == NkControleur::NK_HUMAIN;
					}

					NkLudoPartie mPartie;
					NkVector<NkLudoCoup> mCoups;
					NkLudoGeometrie mGeo;
					NkEcran mEcran = NkEcran::NK_MENU; ///< on commence par CHOISIR
					NkLudoAnim mAnim;
					NkLudoDeAnim mDeAnim;

					/// Par defaut : vous etes le rouge, les trois autres sont l'IA.
					NkControleur mControleur[NK_LUDO_JOUEURS] = {NkControleur::NK_HUMAIN, NkControleur::NK_IA,
																NkControleur::NK_IA, NkControleur::NK_IA};

					bool mDeLance = false;
					int32 mDernierDe = 0;
					bool mFinie = false;
					int32 mGagnant = -1;
					float32 mAttente = 0.f;
					uint32 mGraine = 20260901u;
			};

		} // namespace ludo
	} // namespace jeux
} // namespace nkentseu
