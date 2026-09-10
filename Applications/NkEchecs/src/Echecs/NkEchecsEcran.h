// =============================================================================
// NkEchecsEcran.h — theme, geometrie et dessin. SANS etat de jeu.
//
// A QUOI SERT CE FICHIER
//   Il calcule ou vont les choses et il les dessine. Il ne DECIDE rien : il ne
//   sait ni de qui c'est le tour, ni si un coup est legal. Tout lui arrive par
//   NkEchecsVue.
//
// LES PIECES SONT DESSINEES, PAS ECRITES
//   Pas de police d'echecs, pas d'atlas : chaque piece est une construction
//   geometrique. C'est ce qui permet au jeu de demarrer a l'identique sur les
//   sept plateformes sans embarquer un seul fichier — et une piece dessinee
//   reste nette a toutes les tailles, ce qu'un glyphe rastere ne fait pas.
//   ⚠️ Le critere de dessin n'est pas la beaute, c'est la SILHOUETTE a petite
//   taille : un fou et un pion qui se confondent a 30 px rendent le jeu
//   inutilisable sur telephone.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une couleur        -> la palette ci-dessous
//   - un element visuel  -> une fonction Dessiner*
//   - une zone cliquable -> un NkRect dans la geometrie ; le test du clic
//                           reste chez le jeu
// =============================================================================
#pragma once

#include "Echecs/NkEchecsRegles.h"
#include "NKCanvas/App/NkCanvasApp.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace jeux {
		namespace echecs {

			using nkgui::NkColor;
			using nkgui::NkGuiDrawList;
			using nkgui::NkGuiFont;
			using nkgui::NkRect;
			using math::NkVec2f;

			// --- Palette : la seule source ---------------------------------
			const NkColor kFond(18, 20, 28);
			const NkColor kPanneau(30, 34, 46);
			const NkColor kPanneauActif(52, 58, 74);
			const NkColor kBord(70, 78, 96);
			const NkColor kVoile(8, 10, 16, 205);
			const NkColor kCaseClaire(232, 217, 190);
			const NkColor kCaseSombre(120, 88, 62);
			const NkColor kCadre(48, 34, 24);
			const NkColor kBlanc(248, 246, 242);
			const NkColor kBlancTrait(120, 112, 100);
			const NkColor kNoir(42, 40, 46);
			/// ⚠️ LE LISERE D'UNE PIECE EST PLUS SOMBRE QU'ELLE, JAMAIS PLUS CLAIR.
			/// Mesure du 2026-09-01 : kNoirTrait valait (150,146,156) — plus CLAIR
			/// que le corps noir (42,40,46). Resultat, le contour bas de chaque
			/// piece noire formait une bande argentee la ou se trouve le socle, et
			/// les pieces noires semblaient posees sur une base claire.
			/// J'ai d'abord cru a un socle trop fin, puis a une couleur de socle :
			/// les deux etaient faux. Seule la sonde de pixels a nomme le coupable —
			/// elle a lu (150,146,156) LA OU LE CORPS aurait du etre.
			/// Un lisere sert la SILHOUETTE : sur fond clair, il doit foncer.
			const NkColor kNoirTrait(16, 15, 20);
			const NkColor kSelection(90, 200, 255);
			const NkColor kDestination(90, 220, 140);
			const NkColor kAlerte(235, 80, 80);
			const NkColor kOr(240, 190, 70);
			const NkColor kTexte(236, 238, 245);
			const NkColor kTexteFaible(150, 158, 176);

			/// Qui tient un siege. Seule notion de mode : "contre l'ordinateur",
			/// "a deux" et "simulation" ne sont que trois configurations.
			enum class NkControleur : uint8 { NK_HUMAIN = 0, NK_IA };

			/// L'ecran affiche. Deux seulement : on choisit, puis on joue.
			enum class NkEcran : uint8 { NK_MENU = 0, NK_PARTIE };

			/// Des pieces en mouvement. L'etat de la partie est DEJA a jour :
			/// ceci ne decrit que ce qu'on montre pendant que l'oeil rattrape.
			///
			/// ⚠️ DEUX mouvements, parce que le ROQUE deplace le roi ET la tour.
			/// N'animer que le roi ferait teleporter la tour — et c'est le coup
			/// qu'un debutant comprend le moins.
			struct NkEchecsAnim {
					struct Mouvement {
							NkEchecsPiece piece = NkEchecsPiece::NK_VIDE;
							int8 depR = 0, depC = 0, arrR = 0, arrC = 0;
					};
					bool actif = false;
					float32 t = 0.f;
					float32 duree = 0.22f;
					uint8 nb = 0;
					Mouvement mvt[2];

					/// true si (r,c) est la case d'arrivee d'un mouvement en cours.
					/// Le dessin doit l'y SAUTER : la piece y est deja dans les
					/// regles, elle apparaitrait deux fois.
					// ⚠️ La piece PRISE, gardee en image. Les regles l'ont deja
					// retiree ; sans cette copie, elle s'evapore avant que
					// l'attaquant l'atteigne — la prise se lit a l'envers.
					NkEchecsPiece prise = NkEchecsPiece::NK_VIDE;
					int8 priseR = -1, priseC = -1;

					bool EstArrivee(int32 r, int32 c) const noexcept {
						for (uint8 i = 0; i < nb; ++i) {
							if (mvt[i].arrR == r && mvt[i].arrC == c) {
								return true;
							}
						}
						return false;
					}
			};

			/// Les trois modes du menu. Ils ne sont qu'un raccourci vers une
			/// configuration de sieges — le mode n'est PAS stocke, sinon il
			/// pourrait contredire les bascules du pied de page.
			enum class NkMode : uint8 { NK_CONTRE_ORDI = 0, NK_A_DEUX, NK_IA_CONTRE_IA };

			struct NkEchecsGeometrie {
					NkRect plateau{0.f, 0.f, 0.f, 0.f};
					NkRect bandeau{0.f, 0.f, 0.f, 0.f};
					NkRect siege[2]{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};
					NkRect rejouer{0.f, 0.f, 0.f, 0.f};
					NkRect retour{0.f, 0.f, 0.f, 0.f};
					NkRect choix[3]{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};
					NkRect menuTitre{0.f, 0.f, 0.f, 0.f};
					NkRect menuSousTitre{0.f, 0.f, 0.f, 0.f};
					float32 cellule = 0.f;

					void Calculer(const renderer::NkLayoutInfo &info) noexcept;

					NkVec2f CentreCase(int32 r, int32 c) const noexcept {
						return NkVec2f(plateau.x + (static_cast<float32>(c) + 0.5f) * cellule,
									   plateau.y + (static_cast<float32>(r) + 0.5f) * cellule);
					}
					NkRect CaseRect(int32 r, int32 c) const noexcept {
						return NkRect{plateau.x + static_cast<float32>(c) * cellule,
									  plateau.y + static_cast<float32>(r) * cellule, cellule, cellule};
					}
					bool CaseSous(const NkVec2f &p, int32 &r, int32 &c) const noexcept;
			};

			bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept;

			struct NkEchecsVue {
					const NkEchecsPartie *partie = nullptr;
					const NkVector<NkEchecsCoup> *coupsProposes = nullptr;
					const NkControleur *controleur = nullptr; ///< tableau de 2
					int32 selR = -1, selC = -1;
					NkEchecsEtat etat = NkEchecsEtat::NK_EN_COURS;
					bool finie = false;
					const NkEchecsAnim *anim = nullptr;
			};

			struct NkEchecsPolices {
					NkGuiFont *titre = nullptr;
					NkGuiFont *corps = nullptr;
					NkGuiFont *petite = nullptr;
			};

			void DessinerFond(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info);
			void DessinerMenu(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsPolices &f);
			void DessinerBandeau(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsPolices &f,
								 const NkEchecsVue &vue);
			void DessinerDamier(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsVue &vue);
			void DessinerPieces(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsVue &vue);
			void DessinerPiedDePage(NkGuiDrawList &dl, const NkEchecsGeometrie &geo, const NkEchecsPolices &f,
									const NkEchecsVue &vue);
			void DessinerFin(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkEchecsPolices &f,
							 const NkEchecsVue &vue);

			/// Une piece, en geometrie pure. `type` suit NkEchecsType (1..6).
			void DessinerPiece(NkGuiDrawList &dl, const NkVec2f &centre, float32 cellule, uint8 type, bool blanc);

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
