//
// NkEditeurApp.cpp
// =============================================================================
// Description :
//   L'assemblage de l'editeur sur NkCanvasGuiApp : arguments, polices, theme,
//   entree NKGui, ordre de dessin de la trame, raccourcis.
//
// Caracteristiques :
//   - L'ordre de la trame est porteur de sens (voir OnDraw) : le CORPS se
//     dessine d'abord, avec des gestes neutralises si un menu est ouvert ; la
//     barre de menus et le menu ouvert ensuite, avec l'entree reelle.
//   - La correspondance des touches est une TABLE, recopiee de
//     NkEditorShell::MapEditKey : une ligne par touche, rien a raisonner.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurApp.h"

#include "Editeur/NkEditeurActions.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKWindow/Core/NkWESystem.h"
#include "Unkeny/Banc/NkUnkenyBanc.h"
#include "Unkeny/Banc/NkUnkenyBancEntrees.h"
#include "Unkeny/Banc/NkUnkenyBancLivraison.h"
#include <cstdio>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkGuiKey;

		namespace {

			/// Les gestes de souris que le masquage neutralise, et leur sauvegarde.
			/// On ne copie pas tout NkGuiInput : le clavier et la file de
			/// caracteres ne doivent pas etre rejoues deux fois.
			struct NkGestesSouris {
					nkgui::NkVec2 position{0.f, 0.f};
					bool bas[3] = {};
					bool clic[3] = {};
					bool relache[3] = {};
					bool double_[3] = {};
					float32 molette = 0.f;
					float32 moletteH = 0.f;
			};

			NkGestesSouris Sauver(const nkgui::NkGuiInput &in) {
				NkGestesSouris g;
				g.position = in.mousePos;
				for (int32 i = 0; i < 3; ++i) {
					g.bas[i] = in.mouseDown[i];
					g.clic[i] = in.mouseClicked[i];
					g.relache[i] = in.mouseReleased[i];
					g.double_[i] = in.mouseDoubleClicked[i];
				}
				g.molette = in.wheel;
				g.moletteH = in.wheelH;
				return g;
			}

			void Rendre(nkgui::NkGuiInput &in, const NkGestesSouris &g) {
				in.mousePos = g.position;
				for (int32 i = 0; i < 3; ++i) {
					in.mouseDown[i] = g.bas[i];
					in.mouseClicked[i] = g.clic[i];
					in.mouseReleased[i] = g.relache[i];
					in.mouseDoubleClicked[i] = g.double_[i];
				}
				in.wheel = g.molette;
				in.wheelH = g.moletteH;
			}

			/// ⚠️ ON NEUTRALISE LES GESTES ; LA POSITION, SEULEMENT SUR LE MENU.
			/// Hors du menu, le survol du corps reste juste (il ne clignote pas a
			/// l'ouverture) ; sur le menu, rien dessous ne doit se croire survole.
			/// La position « nulle part » est la sentinelle du depot.
			void Neutraliser(nkgui::NkGuiInput &in, bool surLeMenu) {
				for (int32 i = 0; i < 3; ++i) {
					in.mouseDown[i] = false;
					in.mouseClicked[i] = false;
					in.mouseReleased[i] = false;
					in.mouseDoubleClicked[i] = false;
				}
				in.wheel = 0.f;
				in.wheelH = 0.f;
				if (surLeMenu) {
					in.mousePos = nkgui::NkVec2{-100000.f, -100000.f};
				}
			}

			/// Convention NKGui : [0] gauche, [1] droit, [2] milieu ; -1 sinon.
			int32 IndiceBouton(NkMouseButton b) noexcept {
				switch (b) {
					case NkMouseButton::NK_MB_LEFT:   return 0;
					case NkMouseButton::NK_MB_RIGHT:  return 1;
					case NkMouseButton::NK_MB_MIDDLE: return 2;
					default:                          return -1;
				}
			}

			/// Le curseur que NKGui demande, dans le vocabulaire de la fenetre.
			/// La meme table que NkEditorShell (MapCursor).
			NkWindow::NkCursorType CurseurFenetre(nkgui::NkGuiCursor c) noexcept {
				switch (c) {
					case nkgui::NkGuiCursor::Text:     return NkWindow::NkCursorType::TextInput;
					case nkgui::NkGuiCursor::Hand:     return NkWindow::NkCursorType::Hand;
					case nkgui::NkGuiCursor::ResizeEW: return NkWindow::NkCursorType::ResizeWE;
					case nkgui::NkGuiCursor::ResizeNS: return NkWindow::NkCursorType::ResizeNS;
					default:                           return NkWindow::NkCursorType::Arrow;
				}
			}

			struct NkCorrespondance {
					NkKey os;
					NkGuiKey gui;
			};

			/// Recopie de NkEditorShell::MapEditKey (NkEditorShell.cpp), en table.
			/// ⚠️ UNE TOUCHE ABSENTE D'ICI N'EXISTE PAS POUR NKGUI : KeyPressed()
			///    rendra faux pour toujours, sans erreur nulle part.
			const NkCorrespondance kTouches[] = {
				{NkKey::NK_LEFT, NkGuiKey::Left},		  {NkKey::NK_RIGHT, NkGuiKey::Right},
				{NkKey::NK_UP, NkGuiKey::Up},			  {NkKey::NK_DOWN, NkGuiKey::Down},
				{NkKey::NK_HOME, NkGuiKey::Home},		  {NkKey::NK_END, NkGuiKey::End},
				{NkKey::NK_BACK, NkGuiKey::Backspace},	  {NkKey::NK_DELETE, NkGuiKey::Delete},
				{NkKey::NK_ENTER, NkGuiKey::Enter},		  {NkKey::NK_ESCAPE, NkGuiKey::Escape},
				{NkKey::NK_TAB, NkGuiKey::Tab},			  {NkKey::NK_F2, NkGuiKey::F2},
				{NkKey::NK_F5, NkGuiKey::F5},			  {NkKey::NK_F8, NkGuiKey::F8},
				{NkKey::NK_F12, NkGuiKey::F12},			  {NkKey::NK_SPACE, NkGuiKey::Space},
				{NkKey::NK_BACKSLASH, NkGuiKey::Backslash}, {NkKey::NK_PERIOD, NkGuiKey::Period},
				{NkKey::NK_SLASH, NkGuiKey::Slash},		  {NkKey::NK_LBRACKET, NkGuiKey::LBracket},
				{NkKey::NK_RBRACKET, NkGuiKey::RBracket}, {NkKey::NK_MINUS, NkGuiKey::Minus},
				{NkKey::NK_EQUALS, NkGuiKey::Equal},	  {NkKey::NK_COMMA, NkGuiKey::Comma},
				{NkKey::NK_NUM0, NkGuiKey::Num0},		  {NkKey::NK_NUMPAD_0, NkGuiKey::Num0},
				{NkKey::NK_NUM1, NkGuiKey::Num1},		  {NkKey::NK_NUM2, NkGuiKey::Num2},
				{NkKey::NK_NUM3, NkGuiKey::Num3},		  {NkKey::NK_NUMPAD_3, NkGuiKey::Num3},
				{NkKey::NK_NUM4, NkGuiKey::Num4},		  {NkKey::NK_NUMPAD_4, NkGuiKey::Num4},
				{NkKey::NK_NUM5, NkGuiKey::Num5},		  {NkKey::NK_NUMPAD_5, NkGuiKey::Num5},
				{NkKey::NK_NUM6, NkGuiKey::Num6},		  {NkKey::NK_NUMPAD_6, NkGuiKey::Num6},
				{NkKey::NK_A, NkGuiKey::A},				  {NkKey::NK_B, NkGuiKey::B},
				{NkKey::NK_C, NkGuiKey::C},				  {NkKey::NK_D, NkGuiKey::D},
				{NkKey::NK_E, NkGuiKey::E},				  {NkKey::NK_F, NkGuiKey::F},
				{NkKey::NK_G, NkGuiKey::G},				  {NkKey::NK_H, NkGuiKey::H},
				{NkKey::NK_I, NkGuiKey::I},				  {NkKey::NK_J, NkGuiKey::J},
				{NkKey::NK_K, NkGuiKey::K},				  {NkKey::NK_L, NkGuiKey::L},
				{NkKey::NK_M, NkGuiKey::M},				  {NkKey::NK_N, NkGuiKey::N},
				{NkKey::NK_O, NkGuiKey::O},				  {NkKey::NK_Q, NkGuiKey::Q},
				{NkKey::NK_R, NkGuiKey::R},				  {NkKey::NK_S, NkGuiKey::S},
				{NkKey::NK_T, NkGuiKey::T},				  {NkKey::NK_U, NkGuiKey::U},
				{NkKey::NK_W, NkGuiKey::W},				  {NkKey::NK_Y, NkGuiKey::Y},
				{NkKey::NK_Z, NkGuiKey::Z},
			};

		} // namespace

		// =====================================================================
		NkEditeurApp::NkEditeurApp()
			: mModele(memory::NkMakeUnique<NkEditeurModele>()), mUi(memory::NkMakeUnique<NkEditeurInterface>()),
			  mEntrees(memory::NkMakeUnique<NkEditeurEntrees>()), mConstruction(memory::NkMakeUnique<NkEditeurConstruction>()),
			  mTheme(editorkit::NkTheme::Dark()) {
			NkEditeurEntreesParDefaut(*mEntrees);
			mPalette = NkEditeurPalette(mTheme);
			renderer::NkCanvasAppConfig &cfg = Config();
			cfg.title = "Unkeny — éditeur";
			cfg.width = 1280;
			cfg.height = 760;
			// SANS CADRE, comme NK3DModeler : la barre de titre est peinte par
			// l'editeur, et le menu principal est SUR sa ligne (demande de Rihen,
			// 2026-09-29). Deplacer et redimensionner sont confies a l'OS
			// (NkWindow::BeginDragMove / BeginResize) : voir AppliquerDemandesFenetre.
			cfg.frame = false;
			cfg.fenetre.minWidth = 900;
			cfg.fenetre.minHeight = 560;
			// Le fond de la fenetre est celui du theme : sans cela, le premier
			// cadre (et tout bord non peint) sortirait dans le gris de NKCanvas.
			cfg.clearColor = renderer::NkColor2D{mPalette.fond.r, mPalette.fond.g, mPalette.fond.b, 255};
		}

		// =====================================================================
		NkOptional<int> NkEditeurApp::OnCommandLine(const NkVector<NkString> &args) {
			if (!mModele || !mUi) {
				return NkOptional<int>(-1);
			}
			NkEditeurModele &m = *mModele;
			for (uint32 i = 0; i < args.Size(); ++i) {
				if (args[i].StartsWith("--profil=")) {
					const int32 n = NkString(args[i].SubStr(9)).ToInt32();
					m.profil = (n >= 0 && n < NkNbProfils()) ? n : 0;
					continue;
				}
				if (args[i] == "--paysage") {
					m.paysage = true;
					continue;
				}
				if (args[i] == "--simuler") {
					m.simuler = true;
					continue;
				}
				// --outil= et --selection= : pour qu'une capture d'un GIZMO soit
				// reproductible (--capture=), comme --profil= l'est pour l'appareil.
				// Sans elles, montrer un gizmo demande une souris -- et donc quelqu'un.
				if (args[i].StartsWith("--outil=")) {
					static const char *kNoms[] = {"selection", "poser", "effacer", "saisir", "couteau", "deplacer",
												  "tourner", "echelle"};
					const NkString nom(args[i].SubStr(8));
					for (int32 k = 0; k < 8; ++k) {
						if (nom == NkString(kNoms[k])) {
							m.outil = static_cast<NkOutil>(k);
						}
					}
					continue;
				}
				if (args[i].StartsWith("--selection=")) {
					mSelectionDepart = NkString(args[i].SubStr(12));
					continue;
				}
				// --scene= : ouvrir un .nkscene donne, sans passer par le fichier
				// de l'utilisateur (AppData). Avec --selection= et --capture=,
				// c'est la capture d'un panneau Details sans souris (2026-09-29 :
				// celui de l'Animateur, qu'aucune scene neuve ne porte).
				// --ouvrir= (branche hierarchie, meme jour, meme besoin : capturer
				// l'Outliner en arbre) en est un ALIAS a la fusion — un seul chemin
				// d'ouverture au demarrage, deux noms pour ne casser aucun usage ecrit.
				if (args[i].StartsWith("--scene=") || args[i].StartsWith("--ouvrir=")) {
					mSceneDepart = NkString(args[i].SubStr(args[i].StartsWith("--scene=") ? 8 : 9));
					continue;
				}
				if (args[i] == "--selftest") {
					// Le moteur d'abord (textures, sauvegarde, son, systemes), puis
					// les ACTIONS de l'editeur : un echec d'Unkeny se lit ainsi a
					// sa source, pas dans ses consequences.
					const int32 moteur = unkeny::NkUnkenyLancerBanc();
					const int32 editeur = NkEditeurLancerBanc();
					// Les entrees (29/09) : APRES, et comptees a part, pour que les
					// deux bancs d'avant gardent leurs comptes.
					const int32 entrees = unkeny::NkUnkenyLancerBancEntrees();
					const int32 jouer = NkEditeurLancerBancEntrees();
					// La livraison (U5), comptee a part elle aussi : cuire et relire
					// (moteur), puis preparer une construction (editeur).
					const int32 livraison = unkeny::NkUnkenyLancerBancLivraison();
					const int32 construction = NkEditeurLancerBancLivraison();
					const bool echec = moteur != 0 || editeur != 0 || entrees != 0 || jouer != 0 || livraison != 0 || construction != 0;
					return NkOptional<int>(echec ? 1 : 0);
				}
				// La fenetre « Construire » ouverte des le depart : pour qu'une
				// capture (--capture=) la montre sans souris, comme --outil=.
				if (args[i] == "--fenetre=construire" || args[i] == "--fenetre=construire-auto") {
					mUi->construireDemande = true;
					// « -auto » : le bouton se presse seul, trois trames plus tard.
					mConstruction->lancementAuto = args[i].EndsWith("-auto") ? 3 : 0;
					continue;
				}
				if (args[i].StartsWith("--sortie=")) {
					std::snprintf(mConstruction->sortie, sizeof(mConstruction->sortie), "%s", NkString(args[i].SubStr(9)).CStr());
					continue;
				}
				if (args[i].StartsWith("--nom=")) {
					std::snprintf(mConstruction->nom, sizeof(mConstruction->nom), "%s", NkString(args[i].SubStr(6)).CStr());
					continue;
				}
				// `--construire=PLATEFORME ...` : la construction de la fenetre
				// « Construire », sans fenetre (Livraison/NkEditeurConstruire.h).
				if (args[i].StartsWith("--construire")) {
					return NkOptional<int>(NkEditeurConstruireEnLigne(args));
				}
			}
			return NkOptional<int>();
		}

		// =====================================================================
		float32 NkEditeurApp::TaillePoliceCorps(const renderer::NkLayoutInfo &lay) const noexcept {
			// 13 px a densite 1, comme UE5 et NK3DModeler. La densite de l'ecran
			// (DPI) s'applique : 13 px physiques sur un ecran a 200 % seraient
			// illisibles.
			const float32 d = lay.density < 1.f ? 1.f : (lay.density > 2.5f ? 2.5f : lay.density);
			return 13.f * d;
		}

		// =====================================================================
		bool NkEditeurApp::OnGuiInit() {
			NkEditeurModele &m = *mModele;
			// ── Le theme : celui du kit, converti par SON convertisseur ─────
			nkgui::NkGuiContext &ctx = Gui();
			editorkit::NkThemeVersGui(ctx, mTheme);
			// La geometrie n'est pas une couleur : la conversion ne la touche
			// pas, c'est a l'application de la poser (UI_SPEC §3.3 : coins de
			// 2 px, rangees denses).
			ctx.theme.rounding = 2.f;
			ctx.theme.roundingSmall = 2.f;
			ctx.theme.framePadX = 6.f;
			ctx.theme.framePadY = 3.f;
			ctx.font = FontBody();

			// ── La scene ─────────────────────────────────────────────────────
			// Les textures des acteurs sont FABRIQUEES maintenant, puis partent
			// au rendu de la coquille.
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			m.textures.Brancher(&renderer::NkCanvasGuiApp::RelaisTeleversement, static_cast<renderer::NkCanvasGuiApp *>(this));
			NkEditeurNouvelleScene(m);
			m.carte.Creer(40, 24, 1.f);
			m.carte.AjouterCouche(0, 1.f);
			m.carte.PoserNature(1, NkNatureTuile::NK_SOLIDE);
			if (!mSceneDepart.Empty()) {
				// ⚠️ Le chemin DEVIENT celui de la scene : « Enregistrer » y
				// ecrira, comme apres un Ouvrir. Un echec garde la scene neuve,
				// et l'annonce le dit.
				m.chemin = mSceneDepart;
				NkEditeurOuvrir(m);
			}
			// La scene de depart est la reference « enregistree » : rien n'a
			// encore change, la fermer ne doit rien demander.
			NkEditeurRetenirEmpreinte(m, *mUi);
			if (!mSelectionDepart.Empty()) {
				// La premiere entite dont l'etiquette COMMENCE par ce nom.
				const char *voulu = mSelectionDepart.CStr();
				m.scene.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					usize k = 0;
					while (voulu[k] != '\0' && e.nom[k] == voulu[k]) {
						++k;
					}
					if (voulu[k] == '\0' && !m.aSelection) {
						m.selection = id;
						m.aSelection = true;
					}
				});
			}
			if (m.simuler) {
				NkEditeurJouer(m);
			}
			mUi->cadrageEnAttente = true;
			return true;
		}

		// =====================================================================
		void NkEditeurApp::OnTick(float32 deltaTime) {
			mDernierDt = deltaTime;
			mUi->temps += deltaTime;
			mUi->dt = deltaTime;
			// AU DEBUT de la trame, avant tout dessin : c'est ici que les boucles
			// modales de l'OS (deplacer, redimensionner) peuvent tourner sans
			// reentrer dans une trame a moitie peinte.
			AppliquerDemandesFenetre();
			// Les actions du jeu AVANT le pas : la scene lit l'entree de CETTE trame.
			const bool occupe = mUi->confirmation != NK_A_AUCUNE || mUi->menu != NkMenuEditeur::NK_AUCUN || mConstruction->ouverte;
			NkEditeurEntreesTrame(*mEntrees, mModele->etat, &NkWESystem::Gamepads(), mUi->viseur, occupe);
			// ── LE PAS DE SIMULATION VIT ICI ─────────────────────────────────
			// Avec le shell, il vivait dans le dessin du panneau viseur, et
			// fermer le viseur mettait la simulation en pause. La coquille a un
			// pas de temps a elle : la scene avance, vue ouverte ou non.
			NkEditeurAvancer(*mModele, deltaTime);
			NkEditeurSuivreModifications(*mModele, *mUi, deltaTime);
			// La construction en cours : les lignes de Jenga au Journal, l'etape
			// suivante quand une etape finit (Livraison/NkEditeurFenetreConstruire).
			NkEditeurAvancerConstruction(*mConstruction, *mModele, *mUi);
			// Images COMPTEES sur une demi-seconde. ⚠️ Pas une moyenne glissante
			// de 1/dt : la premiere trame rend un dt quasi nul, et sa valeur
			// (des centaines de milliers) restait visible pendant des secondes
			// (mesure du 2026-09-29 sur la premiere capture : « 101762 ips »).
			mTempsIps += deltaTime;
			++mTramesIps;
			if (mTempsIps >= 0.5f) {
				mUi->ips = static_cast<float32>(mTramesIps) / mTempsIps;
				mTempsIps = 0.f;
				mTramesIps = 0;
			}
		}

		// =====================================================================
		// LA FENETRE SANS CADRE : ce que la barre de titre a demande
		// =====================================================================
		void NkEditeurApp::AppliquerDemandesFenetre() {
			NkEditeurInterface &ui = *mUi;
			NkWindow &fenetre = Window();
			nkgui::NkGuiInput &in = Gui().input;
			if (ui.reduireDemande) {
				ui.reduireDemande = false;
				fenetre.Minimize();
			}
			if (ui.agrandirDemande) {
				std::printf("[trace] agrandir applique, etait agrandie=%d\n", fenetre.IsMaximized() ? 1 : 0); std::fflush(stdout);
				ui.agrandirDemande = false;
				if (fenetre.IsMaximized()) {
					fenetre.Restore();
				} else {
					fenetre.Maximize();
				}
			}
			if (ui.deplacerDemande) {
				ui.deplacerDemande = false;
				// TIRER UNE FENETRE AGRANDIE LA RESTAURE, puis la deplace -- le geste
				// de toutes les fenetres du systeme. On la replace SOUS LE CURSEUR,
				// a la meme fraction de largeur, sinon elle saute en haut a gauche
				// et la suite du geste l'emmene ailleurs. La recette est celle de
				// NK3DModeler (main.cpp, « ACTIONS DE FENETRE, HORS FRAME »).
				if (fenetre.IsMaximized()) {
					const nkgui::NkVec2 souris = in.mousePos;
					fenetre.Restore();
					const math::NkVec2u taille = fenetre.GetSize();
					const int32 nx = static_cast<int32>(souris.x - static_cast<float32>(taille.x) * ui.deplacerFractionX);
					const int32 ny = static_cast<int32>(souris.y - 12.f);
					fenetre.SetPosition(nx < 0 ? 0 : nx, ny < 0 ? 0 : ny);
				}
				fenetre.BeginDragMove();
				// ⚠️ LA BOUCLE MODALE A MANGE LE RELACHEMENT : sans cette ligne, le
				//    bouton resterait « enfonce » pour NKGui apres le deplacement, et
				//    le prochain survol deplacerait une cloison ou une entite.
				in.mouseDown[0] = false;
			}
			if (ui.redimDemande >= 0) {
				const NkWindow::NkResizeEdge bord = static_cast<NkWindow::NkResizeEdge>(ui.redimDemande);
				ui.redimDemande = -1;
				fenetre.BeginResize(bord);
				in.mouseDown[0] = false; // meme raison que pour le deplacement
			}
		}

		// =====================================================================
		// L'ENTREE : NkEditorShell::HookEvents, recopie ici
		// =====================================================================
		void NkEditeurApp::MapperTouche(NkKey touche, bool enfoncee) noexcept {
			nkgui::NkGuiInput &in = Gui().input;
			for (const NkCorrespondance &c : kTouches) {
				if (c.os == touche) {
					in.SetKey(c.gui, enfoncee);
				}
			}
		}

		bool NkEditeurApp::OnEvent(const NkEvent &event) {
			// La vue active EN JEU : le clavier, la manette et le doigt sont au
			// jeu (NkEditeurEntrees.h). Ce qu'il prend, NKGui ne le voit pas.
			const bool occupe = mUi->confirmation != NK_A_AUCUNE || mUi->menu != NkMenuEditeur::NK_AUCUN || mConstruction->ouverte;
			if (NkEditeurEntreesEvenement(*mEntrees, mModele->etat, event, mUi->viseur, occupe)) {
				return false;
			}
			nkgui::NkGuiInput &in = Gui().input;
			if (const auto *e = event.As<NkMouseMoveEvent>()) {
				in.mousePos = nkgui::NkVec2{static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
				return false;
			}
			if (const auto *e = event.As<NkMouseButtonPressEvent>()) {
				// Convention NKGui : [0] gauche, [1] droit, [2] milieu.
				in.mousePos = nkgui::NkVec2{static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
				const int32 b = IndiceBouton(e->GetButton());
				if (b >= 0) {
					in.mouseDown[b] = true;
					std::printf("[trace] appui b=%d a (%d,%d)\n", b, (int)e->GetX(), (int)e->GetY()); std::fflush(stdout);
					mAppuiNonVu[b] = true;
				}
				in.ctrlDown = e->GetModifiers().ctrl;
				in.shiftDown = e->GetModifiers().shift;
				in.altDown = e->GetModifiers().alt;
				return false;
			}
			if (const auto *e = event.As<NkMouseButtonReleaseEvent>()) {
				const int32 b = IndiceBouton(e->GetButton());
				if (b < 0) {
					return false;
				}
				// ⚠️ LE CLIC PLUS COURT QU'UNE TRAME. NKGui derive le clic de la
				//    transition de `mouseDown` d'une trame a l'autre : un appui ET son
				//    relachement arrives entre deux trames (pave tactile, clic sec)
				//    laissaient `mouseDown` a faux des deux cotes -- le clic
				//    n'existait pas. Le relachement attend donc que la trame ait VU
				//    l'appui ; il est applique a la fin de OnDraw.
				if (mAppuiNonVu[b]) {
					mRelacheDiffere[b] = true;
				} else {
					in.mouseDown[b] = false;
				}
				return false;
			}
			if (const auto *e = event.As<NkMouseDoubleClickEvent>()) {
				// Le double-clic de l'OS, injecte : il sert a l'Outliner (cadrer).
				in.mousePos = nkgui::NkVec2{static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
				if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
					in.SetDoubleClick(0);
				}
				return false;
			}
			if (const auto *e = event.As<NkMouseWheelVerticalEvent>()) {
				in.wheel += static_cast<float32>(e->GetDeltaY());
				in.ctrlDown = e->GetModifiers().ctrl;
				in.shiftDown = e->GetModifiers().shift;
				in.altDown = e->GetModifiers().alt;
				return false;
			}
			if (const auto *e = event.As<NkMouseWheelHorizontalEvent>()) {
				in.wheelH += static_cast<float32>(e->GetDeltaX());
				return false;
			}
			if (const auto *e = event.As<NkTextInputEvent>()) {
				in.PushChar(e->GetCodepoint());
				return false;
			}
			if (const auto *e = event.As<NkKeyPressEvent>()) {
				const NkKey k = e->GetKey();
				MapperTouche(k, true);
				in.ctrlDown = e->GetModifiers().ctrl;
				in.shiftDown = e->GetModifiers().shift;
				in.altDown = e->GetModifiers().alt;
				if (e->GetModifiers().ctrl) {
					// Copier / couper / coller / tout selectionner des champs.
					if (k == NkKey::NK_C) {
						in.wantCopy = true;
					} else if (k == NkKey::NK_X) {
						in.wantCut = true;
					} else if (k == NkKey::NK_V) {
						in.wantPaste = true;
					} else if (k == NkKey::NK_A) {
						in.wantSelectAll = true;
					}
				}
				return false;
			}
			if (const auto *e = event.As<NkKeyReleaseEvent>()) {
				MapperTouche(e->GetKey(), false);
				in.ctrlDown = e->GetModifiers().ctrl;
				in.shiftDown = e->GetModifiers().shift;
				in.altDown = e->GetModifiers().alt;
				return false;
			}
			// Rendre false laisse la coquille faire son travail type (fermeture,
			// taille, cycle de vie) : l'editeur n'en consomme aucun.
			return false;
		}

		void NkEditeurApp::Raccourcis(NkEditeurCadre &c) {
			nkgui::NkGuiContext &ctx = Gui();
			const nkgui::NkGuiInput &in = ctx.input;
			if (in.KeyPressed(NkGuiKey::Escape) && c.ui.menu != NkMenuEditeur::NK_AUCUN) {
				c.ui.menu = NkMenuEditeur::NK_AUCUN;
				return;
			}
			// Un champ de saisie focalise garde ses touches : Suppr efface une
			// lettre, pas l'entite ; Espace s'ecrit, il ne lance pas la scene.
			if (ctx.inputId != nkgui::NKGUI_ID_NONE || c.ui.filtreFocus || c.ui.nomFocus) {
				return;
			}
			if (in.ctrlDown) {
				struct NkRaccourci {
						NkGuiKey touche;
						int32 action;
				};
				static const NkRaccourci kCtrl[] = {
					{NkGuiKey::N, NK_A_NOUVEAU},		   {NkGuiKey::O, NK_A_OUVRIR},	  {NkGuiKey::S, NK_A_ENREGISTRER},
					{NkGuiKey::E, NK_A_NOUVELLE_ENTITE}, {NkGuiKey::D, NK_A_DUPLIQUER}, {NkGuiKey::Q, NK_A_QUITTER},
				};
				for (const NkRaccourci &r : kCtrl) {
					if (in.KeyPressed(r.touche)) {
						NkEditeurExecuter(c, r.action);
					}
				}
				return;
			}
			// Une touche, une action : la table d'aiguillage, comme pour Ctrl.
			// Q / W / E / R sont ceux d'UE5 (selection, deplacer, tourner, echelle).
			struct NkTouche {
					NkGuiKey touche;
					int32 action;
			};
			static const NkTouche kSimples[] = {
				{NkGuiKey::Delete, NK_A_SUPPRIMER},
				{NkGuiKey::Space, NK_A_JOUER},
				{NkGuiKey::Escape, NK_A_ARRETER},
				{NkGuiKey::F, NK_A_CADRER_SELECTION},
				{NkGuiKey::F2, NK_A_RENOMMER},
				{NkGuiKey::Q, NK_A_OUTIL + static_cast<int32>(NkOutil::NK_SELECTION)},
				{NkGuiKey::W, NK_A_OUTIL + static_cast<int32>(NkOutil::NK_DEPLACER)},
				{NkGuiKey::E, NK_A_OUTIL + static_cast<int32>(NkOutil::NK_TOURNER)},
				{NkGuiKey::R, NK_A_OUTIL + static_cast<int32>(NkOutil::NK_ECHELLE)},
			};
			for (const NkTouche &t : kSimples) {
				if (in.KeyPressed(t.touche)) {
					NkEditeurExecuter(c, t.action);
				}
			}
		}

		// =====================================================================
		// LA TRAME
		// =====================================================================
		void NkEditeurApp::OnDraw(nkgui::NkGuiDrawList &dl) {
			nkgui::NkGuiContext &ctx = Gui();
			NkEditeurInterface &ui = *mUi;
			const renderer::NkLayoutInfo &lay = Layout();
			NkEditeurPlanifier(ui, static_cast<float32>(lay.width), static_cast<float32>(lay.height));
			NkEditeurCadre c{ctx, *mModele, ui, mTheme, mPalette, FontBody(), FontSmall()};
			dl.AddRectFilled(ui.ecran, mPalette.fond);
			ui.fenetreAgrandie = Window().IsMaximized();
			// ── 0. LES BORDS DE LA FENETRE, avant tout, avec l'entree reelle ─
			// Un clic au bord redimensionne, et n'atteint rien d'autre : la
			// fonction le consomme avant que le corps et les menus ne le lisent.
			if (ui.construireDemande) {
				ui.construireDemande = false;
				NkEditeurOuvrirConstruire(*mConstruction, *mModele);
			}
			if (ui.confirmation == NK_A_AUCUNE && !mConstruction->ouverte) {
				NkEditeurBordsFenetre(c);
			}

			// ── 1. LE CORPS, gestes neutralises si un menu est ouvert ────────
			// ⚠️ LE MENU EST DECIDE AVANT LE CORPS, avec le rectangle de la trame
			//    d'avant. Le corps est peint en premier (le menu doit passer
			//    par-dessus), donc c'est MAINTENANT qu'il faut savoir si le clic
			//    lui revient. Un clic hors du menu le ferme et ne traverse pas :
			//    c'est le geste de tout logiciel de bureau.
			// La croix de la FENETRE : la question se pose ici, ou l'on dessine.
			if (ui.fermetureDemandee) {
				ui.fermetureDemandee = false;
				NkEditeurExecuter(c, NK_A_QUITTER);
			}
			// ⚠️ LA BOITE « NON ENREGISTREE » EST MODALE : tant qu'elle attend,
			//    RIEN d'autre ne recoit la souris -- ni le corps, ni les menus. Et
			//    les champs perdent le focus : sinon Entree validerait le nom en
			//    meme temps que la boite.
			// La fenetre « Construire » est modale de la meme facon.
			const bool modale = ui.confirmation != NK_A_AUCUNE || mConstruction->ouverte;
			if (modale) {
				ui.menu = NkMenuEditeur::NK_AUCUN;
				ui.nomFocus = false;
				ui.filtreFocus = false;
			}
			const NkMenuEditeur menuDebut = ui.menu;
			const NkGestesSouris vrais = Sauver(ctx.input);
			if (modale) {
				Neutraliser(ctx.input, true);
			} else if (menuDebut != NkMenuEditeur::NK_AUCUN) {
				const bool surSous = ui.sousMenu != NkMenuEditeur::NK_AUCUN && NkEditeurDans(ui.sousMenuRect, vrais.position);
				Neutraliser(ctx.input, NkEditeurDans(ui.menuRect, vrais.position) || surSous);
			}
			NkEditeurDessinerVue(c);
			NkEditeurDessinerOutliner(c);
			NkEditeurDessinerDetails(c);
			NkEditeurDessinerTiroir(c);
			NkEditeurCloisons(c);
			NkEditeurDessinerBarreOutils(c);
			NkEditeurDessinerStatut(c);
			// Les onglets sont sous la barre de titre, donc SOUS ses menus
			// deroulants : ils suivent le masquage du corps.
			NkEditeurDessinerOnglets(c);
			// Le bandeau « le jeu a la main » : SOUS les menus, qui passent dessus.
			NkEditeurDessinerEntrees(ctx.dl, FontSmall(), *mEntrees, ui.viseur);

			// ── 2. LES MENUS, avec l'entree reelle (sauf sous la boite) ──────
			if (!modale) {
				Rendre(ctx.input, vrais);
			}
			NkEditeurDessinerBarreMenus(c);
			NkEditeurDessinerMenuOuvert(c, menuDebut);

			// ── 3. La boite, par-dessus tout, avec l'entree reelle ───────────
			Rendre(ctx.input, vrais);
			if (modale) {
				NkEditeurDessinerConfirmation(c);
				NkEditeurDessinerConstruire(c, *mConstruction);
			} else {
				// Le clavier, apres tout ce qui pouvait le prendre. Sous la boite,
				// Entree et Echap sont a elle.
				Raccourcis(c);
			}
			NkEditeurJournaliser(*mModele, ui);

			// ── 4. Ce que la trame laisse a l'OS et a la suivante ────────────
			// Le curseur que les widgets ont demande (cloisons, champs, DragFloat).
			// ⚠️ APPLIQUE ICI, PAS DANS NkCanvasGuiApp : la coquille sert aussi les
			//    jeux, dont les widgets NKGui demandent deja des curseurs que
			//    personne ne leur a jamais montres. Les leur montrer serait un
			//    changement de comportement que ces jeux n'ont pas choisi.
			Window().SetCursor(CurseurFenetre(ctx.wantCursor));
			// Un liseret autour de la fenetre sans cadre : sans lui, son bord se
			// perd sur un bureau sombre. Agrandie, elle n'a pas de bord.
			if (!ui.fenetreAgrandie) {
				dl.AddRect(ui.ecran, mPalette.bord, 1.f);
			}
			// La trame a VU les appuis ; les relachements retenus partent pour la
			// suivante (voir OnEvent).
			for (int32 b = 0; b < 3; ++b) {
				mAppuiNonVu[b] = false;
				if (mRelacheDiffere[b]) {
					mRelacheDiffere[b] = false;
					ctx.input.mouseDown[b] = false;
				}
			}
			if (ui.demandeQuitter) {
				ui.demandeQuitter = false;
				Quit();
			}
		}

		bool NkEditeurApp::OnCloseRequested() {
			// Refuser ici, demander a la trame suivante : la question a besoin de
			// l'ecran, et ce rappel n'en a pas. Si la scene n'a pas change,
			// NK_A_QUITTER ferme aussitot (une trame plus tard).
			mUi->fermetureDemandee = true;
			return false;
		}

	} // namespace editeur
} // namespace nkentseu
