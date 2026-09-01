// =============================================================================
// NkLudoEcran.h — theme, geometrie et dessin. SANS etat de jeu.
//
// A QUOI SERT CE FICHIER
//   Il place et il dessine. Il ne DECIDE rien : tout lui arrive par NkLudoVue.
//
// ⚠️ IL NE REDEFINIT PAS LA GEOMETRIE DU PLATEAU. Les tables de piste, de
//   maison et d'ecurie vivent dans NkLudoRegles, parce que la REGLE et le
//   DESSIN doivent lire les memes. Deux tables auraient fini par diverger, et
//   un pion se serait affiche ailleurs qu'il n'est — defaut qu'on chercherait
//   dans la regle alors qu'il serait dans l'affichage.
//   Ce fichier ne fait que convertir (ligne, colonne) en pixels.
//
// QUATRE JOUEURS, DONC QUATRE SIEGES
//   Chaque siege est tenu par un humain ou une IA. Cela couvre tout : un joueur
//   contre trois IA, deux amis contre deux IA, quatre humains, ou quatre IA qui
//   se disputent la partie.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une couleur         -> la palette ci-dessous
//   - un element visuel   -> une fonction Dessiner*
//   - une case du plateau -> NkLudoRegles, jamais ici
// =============================================================================
#pragma once

#include "Ludo/NkLudoRegles.h"
#include "NKCanvas/App/NkCanvasApp.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace jeux {
		namespace ludo {

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
			const NkColor kPlateau(246, 243, 236);
			const NkColor kTrait(70, 66, 60);
			const NkColor kSure(214, 208, 194);
			const NkColor kTexte(236, 238, 245);
			const NkColor kTexteFaible(150, 158, 176);
			const NkColor kOr(240, 190, 70);

			/// ⚠️ Une couleur par joueur, et elles doivent rester distinctes MEME
			/// pour un daltonien : rouge / bleu / jaune / vert se separent aussi
			/// par leur CLARTE, ce que rouge / vert seuls ne feraient pas.
			const NkColor kJoueur[NK_LUDO_JOUEURS] = {NkColor(214, 62, 62), NkColor(56, 122, 210),
													  NkColor(226, 176, 40), NkColor(60, 172, 96)};
			const NkColor kJoueurSombre[NK_LUDO_JOUEURS] = {NkColor(140, 30, 30), NkColor(28, 70, 138),
															NkColor(150, 110, 12), NkColor(28, 110, 56)};
			const char *const kNomJoueur[NK_LUDO_JOUEURS] = {"Rouge", "Bleu", "Jaune", "Vert"};

			// ⚠️ `NK_DESACTIVE` EST AJOUTE EN DERNIER, PAS EN PREMIER.
			// Une enumeration initialisee par defaut vaut 0 ; mettre
			// `NK_DESACTIVE` a cette place aurait rendu desactive tout siege
			// qu'on avait oublie de regler — un changement de comportement
			// silencieux dans du code qui compilait toujours.
			enum class NkControleur : uint8 { NK_HUMAIN = 0, NK_IA, NK_DESACTIVE };

			/// Le libelle d'un etat de siege, pour l'affichage et les journaux.
			inline const char *NkNomControleur(NkControleur c) noexcept {
				return (c == NkControleur::NK_HUMAIN) ? "Humain"
													  : ((c == NkControleur::NK_IA) ? "IA" : "Desactive");
			}

			/// L'etat suivant dans le cycle : Humain -> IA -> Desactive -> Humain.
			inline NkControleur NkControleurSuivant(NkControleur c) noexcept {
				if (c == NkControleur::NK_HUMAIN) {
					return NkControleur::NK_IA;
				}
				if (c == NkControleur::NK_IA) {
					return NkControleur::NK_DESACTIVE;
				}
				return NkControleur::NK_HUMAIN;
			}
			enum class NkEcran : uint8 { NK_MENU = 0, NK_PARTIE };

			/// Cinq configurations proposees au menu : de un a quatre humains,
			/// plus la simulation. Elles ne sont qu'un raccourci vers un tableau
			/// de sieges — le mode n'est PAS stocke, sinon il contredirait les
			/// bascules du pied de page.
			enum class NkMode : uint8 {
				NK_UN_JOUEUR = 0,
				NK_DEUX_JOUEURS,
				NK_TROIS_JOUEURS,
				NK_QUATRE_JOUEURS,
				NK_SIMULATION
			};
			static const int32 NK_LUDO_NB_MODES = 5;

			/// Les lignes de l'ecran de configuration : QUATRE SIEGES, puis le
			/// bouton « Commencer ». Le compte vaut 5 comme l'ancien nombre de
			/// prereglages -- coincidence, et c'est pour cela qu'il a son propre
			/// nom : deux quantites egales aujourd'hui qui partagent un nom
			/// divergent au premier changement, sans que rien ne l'annonce.
			static const int32 NK_LUDO_NB_LIGNES_MENU = NK_LUDO_JOUEURS + 1;
			static const int32 NK_LUDO_LIGNE_COMMENCER = NK_LUDO_JOUEURS;

			/// Nombre maximal de cases parcourues en une animation : six pas de
			/// de, plus la case de depart.
			static const int32 NK_LUDO_ANIM_MAX = 8;

			/// Un pion en mouvement. L'etat de la partie est DEJA a jour.
			///
			/// ⚠️ Le pion suit SON CHEMIN, case par case : l'interpoler en droite
			/// le ferait traverser le plateau par-dessus les ecuries et le centre.
			struct NkLudoAnim {
					bool actif = false;
					float32 t = 0.f;
					float32 duree = 0.f;
					int8 joueur = -1;
					int8 pion = -1;
					uint8 nbCases = 0;			 ///< cases traversees, depart compris
					int8 ligne[NK_LUDO_ANIM_MAX] = {};
					int8 colonne[NK_LUDO_ANIM_MAX] = {};

					/// Position courante en coordonnees de GRILLE (fractionnaires).
					void Position(float32 &l, float32 &c) const noexcept;
			};

			/// Le de en train de rouler. La VRAIE valeur est deja tiree : ceci ne
			/// fait que retarder son affichage. Une animation qui deciderait du
			/// resultat serait une seconde source de hasard, invisible au banc.
			struct NkLudoDeAnim {
					bool actif = false;
					float32 restant = 0.f;
					int32 faceMontree = 1;
			};

			struct NkLudoGeometrie {
					NkRect plateau{0.f, 0.f, 0.f, 0.f}; ///< carre, 15x15 cases
					NkRect bandeau{0.f, 0.f, 0.f, 0.f};
					NkRect bouton{0.f, 0.f, 0.f, 0.f};	///< lancer le de / passer
					NkRect retour{0.f, 0.f, 0.f, 0.f};	///< retour au menu
					NkRect siege[NK_LUDO_JOUEURS]{};
					NkRect choix[NK_LUDO_NB_MODES]{};
					NkRect menuTitre{0.f, 0.f, 0.f, 0.f};
					NkRect menuSousTitre{0.f, 0.f, 0.f, 0.f};
					float32 cellule = 0.f;

					void Calculer(const renderer::NkLayoutInfo &info) noexcept;

					NkRect CaseRect(int32 ligne, int32 colonne) const noexcept {
						return NkRect{plateau.x + static_cast<float32>(colonne) * cellule,
									  plateau.y + static_cast<float32>(ligne) * cellule, cellule, cellule};
					}
					NkVec2f CentreCase(int32 ligne, int32 colonne) const noexcept {
						return NkVec2f(plateau.x + (static_cast<float32>(colonne) + 0.5f) * cellule,
									   plateau.y + (static_cast<float32>(ligne) + 0.5f) * cellule);
					}
			};

			bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept;

			/// Ou se dessine un pion selon son avancement. UNE fonction pour les
			/// trois cas — ecurie, piste, maison — parce que le dessin et la
			/// regle lisent la meme geometrie.
			void NkPositionPion(int32 joueur, int32 pion, int32 avancement, int32 &ligne, int32 &colonne) noexcept;

			struct NkLudoVue {
					const NkLudoPartie *partie = nullptr;
					const NkVector<NkLudoCoup> *coups = nullptr;
					const NkControleur *controleur = nullptr; ///< tableau de 4
					bool deLance = false;
					int32 dernierDe = 0;
					const NkLudoAnim *anim = nullptr;
					const NkLudoDeAnim *deAnim = nullptr;
					bool finie = false;
					int32 gagnant = -1;
			};

			struct NkLudoPolices {
					NkGuiFont *titre = nullptr;
					NkGuiFont *corps = nullptr;
					NkGuiFont *petite = nullptr;
			};

			void DessinerFond(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info);
			/// L'ecran de configuration : une ligne par siege, puis « Commencer ».
			///
			/// `controleur` est le tableau de quatre etats ; `peutCommencer` dit si
			/// le bouton agit -- il n'agit pas sous deux sieges utilisables.
			void DessinerMenu(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoPolices &f,
							  const NkControleur *controleur, bool peutCommencer);
			void DessinerBandeau(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoPolices &f,
								 const NkLudoVue &vue);
			void DessinerPlateau(NkGuiDrawList &dl, const NkLudoGeometrie &geo);
			void DessinerPions(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoVue &vue);
			void DessinerPiedDePage(NkGuiDrawList &dl, const NkLudoGeometrie &geo, const NkLudoPolices &f,
									const NkLudoVue &vue);
			void DessinerFin(NkGuiDrawList &dl, const renderer::NkLayoutInfo &info, const NkLudoPolices &f,
							 const NkLudoVue &vue);

		} // namespace ludo
	} // namespace jeux
} // namespace nkentseu
