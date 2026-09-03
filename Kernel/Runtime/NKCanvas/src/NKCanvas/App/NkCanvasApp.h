// =============================================================================
// NkCanvasApp.h — COQUILLE D'APPLICATION 2D sur NKCanvas
//
// A QUOI SERT CE FICHIER
//   Il y a DEUX manieres d'ecrire une application NKCanvas, et les deux
//   restent legitimes :
//
//     CLASSIQUE  on ecrit son nkmain, sa fenetre, sa boucle. Maitrise totale.
//                C'est ce que font Pong, Gemcrush, Songoo aujourd'hui.
//     MODERNE    on herite de NkCanvasApp et on remplit des methodes. La
//                fenetre, la boucle et le cycle de vie mobile sont deja faits.
//
//   Ce fichier apporte la seconde. Il ne retire pas la premiere, et il ne se
//   substitue a rien : c'est du code que l'on peut ne pas inclure.
//
// CE QU'ELLE PREND EN CHARGE, ET POURQUOI CE SONT CELLES-LA
//   Mesure du 2026-09-01 sur les 14 applications a fenetre du depot :
//     - 14 creent leur NkRenderWindow a la main
//     - 11 ecrivent leur propre boucle
//     -  7 gerent le cycle de vie mobile — et SEPT SUR QUATORZE seulement,
//        c'est-a-dire que la moitie des applications reviennent d'arriere-plan
//        sur un ECRAN NOIR sans que rien ne le signale.
//   Ce dernier chiffre est la vraie raison d'etre du fichier. Le reste est du
//   confort ; ca, c'est un defaut que l'on ne peut pas voir depuis sa machine
//   de developpement.
//
// CE QU'ELLE NE PREND PAS EN CHARGE, ET C'EST DELIBERE
//   - LE MULTI-FENETRE. Mesure : ZERO application de production du depot ouvre
//     deux fenetres (seuls quatre fichiers de demo de Sandbox le font). Et le
//     mobile n'a pas de multi-fenetre du tout : tout ce que la coquille prend
//     en charge — pause, reprise, surface detruite, zone sure — est par nature
//     mono-fenetre. Concevoir maintenant pour un cas que personne n'a, ce
//     serait figer une forme sans le moindre retour.
//     LA PORTE A UN NOM : le jour ou il en faut deux, on n'alourdit pas cette
//     classe — on ajoute une NkCanvasView (fenetre + cible + ses virtuelles)
//     dont l'application tient une liste. NKEvent route DEJA par NkWindowId
//     (AddEventCallback(cb, windowId)), donc le routage est acquis.
//     En attendant, une application multi-fenetre garde la maniere classique.
//   - NKGUI. Voir NkCanvasGuiApp.h, en-tete SEUL. Poser NKGui ici forcerait
//     tout dependant de NKCanvas a le compiler — exactement le defaut qui a
//     fait que NKCanvas liait NKUI (module deprecie) pendant des mois.
//
// LES EVENEMENTS — trois entrees, UN SEUL CHEMIN
//   niveau 0  NKEvent inchange. AddEventCallback continue de fonctionner sans
//             que la coquille intervienne : mesure faite, DispatchToCallbacks
//             est appele depuis DeliverOnPumpThread, donc A L'EMPILEMENT, et
//             ne passe jamais par PollEvent. La coquille peut vider la file
//             sans jamais affamer un callback.
//   niveau 1  OnEvent(const NkEvent&) -> bool. Le brut. true = consomme.
//   niveau 2  OnPointer / OnKey / OnResize. DERIVES du niveau 1, jamais d'un
//             second abonnement : deux abonnements paralleles qui doivent
//             s'accorder sans que l'un soit la reference de l'autre, c'est le
//             defaut que ce depot paie en boucle.
//
//   ⚠️ LE CONTRAT QUI VA AVEC : avec la coquille, l'application n'appelle PLUS
//   PollEvent elle-meme. La coquille depile ; un second depileur ne verrait
//   rien. Les callbacks, eux, restent entierement libres.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un reglage de demarrage      -> NkCanvasAppConfig
//   - un evenement typé de plus    -> une virtuelle On*, PLUS son aiguillage
//                                     dans NkCanvasApp::DispatchTyped (.cpp).
//                                     Ne jamais l'abonner separement.
//   - un besoin propre a une appli -> chez l'application, pas ici.
// =============================================================================
#pragma once

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCore/NkOptional.h"
#include "NKCore/NkTypes.h"
#include "NKEvent/NkPointerEvent.h"
#include "NKEvent/NkSafeArea.h"
#include "NKMath/NKMath.h"
#include "NKWindow/Core/NkEntry.h"
#include "NKTime/NkChrono.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/NKWindow.h"

namespace nkentseu {
	class NkKeyPressEvent;
	class NkKeyReleaseEvent;

	namespace renderer {

		/// Reglages lus UNE FOIS, avant la creation de la fenetre.
		///
		/// A remplir dans le constructeur de la classe derivee : c'est le seul
		/// moment ou la fenetre n'existe pas encore. Modifier un champ apres
		/// coup n'a aucun effet — et le journal le dit plutot que de l'ignorer.
		struct NkCanvasAppConfig {
				NkString title = "Application NKCanvas";
				uint32 width = 960;
				uint32 height = 540;
				bool resizable = true;
				bool centered = true;
				bool frame = true; ///< false = sans bordure (barre de titre dessinee par l'app)

				/// NK_GFX_API_NONE = "choisis pour moi" : la coquille resout
				/// selon la plateforme, apres avoir consulte la ligne de commande
				/// et l'environnement. Poser une valeur precise ici la propose
				/// comme DEFAUT DE L'APPLICATION — la ligne de commande et
				/// l'environnement restent prioritaires.
				///
				/// ⚠️ NE PAS y mettre NK_GFX_API_AUTO : cette valeur existe dans
				/// l'enumeration et AUCUNE fabrique ne sait la resoudre. Elle
				/// rendait "Unknown or unsupported API: None" et fermait la
				/// fenetre aussitot ouverte.
				NkGraphicsApi backend = NkGraphicsApi::NK_GFX_API_NONE;

				NkColor2D clearColor = NkColor2D{18, 18, 24, 255};

				/// Plafond du pas de temps. Au retour de veille, l'horloge rend
				/// plusieurs secondes d'un coup : sans plafond, tout ce qui
				/// s'integre traverse les murs en une trame.
				float32 maxDeltaTime = 0.1f;

				/// Cadence visee, en images par seconde. 0 = sans plafond.
				///
				/// ⚠️ SUR LE WEB, LA BOUCLE CEDE LA MAIN A CHAQUE TRAME, QUE LE
				/// PLAFOND SOIT ACTIF OU NON. Une boucle `while` qui ne rend
				/// jamais la main au navigateur GELE L'ONGLET : le wasm demarre,
				/// la boucle part, et plus rien ne se peint. C'est le defaut
				/// signale par Rodolf le 2026-09-01 sur le .bat Web.
				///
				/// Le mecanisme existait et etait juste — NkChrono::Sleep appelle
				/// emscripten_sleep(ms) et YieldThread appelle emscripten_sleep(0),
				/// tous deux avec ASYNCIFY deja actif. Ce qui manquait, c'etait
				/// l'APPEL. Mesure du jour : Pong etait la SEULE application du
				/// depot a plafonner sa cadence, et la seule a tourner sur le Web.
				int32 imagesParSeconde = 60;
		};

		/// Ce que la coquille a deja lu pour l'application a chaque changement
		/// de taille — rotation comprise.
		struct NkLayoutInfo {
				uint32 width = 0;
				uint32 height = 0;
				NkSafeAreaInsets safeArea; ///< encoche, barre de statut, indicateur de geste
				float32 density = 1.f;	   ///< facteur d'echelle de l'ecran (DPI)

				bool IsPortrait() const noexcept {
					return height >= width;
				}
		};

		// =====================================================================
		// NkCanvasApp
		// =====================================================================
		class NkCanvasApp {
			public:
				NkCanvasApp() = default;
				virtual ~NkCanvasApp() = default;

				NkCanvasApp(const NkCanvasApp &) = delete;
				NkCanvasApp &operator=(const NkCanvasApp &) = delete;

				/// Point d'entree de la maniere moderne.
				///
				/// ⚠️ ON NE DEFINIT PAS DE NOUVEAU main. `nkmain` EST deja le
				/// point d'entree portable du depot (NkEntry.h:311), avec seize
				/// points d'entree de plateforme derriere lui. L'application
				/// garde donc son nkmain et delegue en une ligne :
				///
				///     int nkmain(const NkEntryState &state) {
				///         return renderer::NkCanvasApp::Run<MonJeu>(state);
				///     }
				///
				/// Ecrire son nkmain n'est pas une corvee, c'est ce qui permet
				/// a une application de repondre AVANT toute fenetre — c'est
				/// ainsi que le banc de Gemcrush tourne sans ecran ni GPU.
				/// OnCommandLine() offre le meme point d'arret ici.
				template <typename T> static int Run(const NkEntryState &state) {
					T app;
					return static_cast<NkCanvasApp &>(app).Execute(state);
				}

				// --- Services offerts a l'application -------------------------
				NkCanvasAppConfig &Config() noexcept {
					return mConfig;
				}
				const NkCanvasAppConfig &Config() const noexcept {
					return mConfig;
				}

				NkWindow &Window() noexcept {
					return mWindow;
				}
				NkRenderWindow &Target() noexcept {
					return *mTarget;
				}
				const NkLayoutInfo &Layout() const noexcept {
					return mLayout;
				}

				/// Secondes ecoulees depuis OnInit, plafond applique.
				float32 Time() const noexcept {
					return mTime;
				}

				/// Demande l'arret. La trame courante se termine proprement.
				void Quit() noexcept {
					mRunning = false;
				}

				/// true entre OnPause et OnResume : la surface native n'existe
				/// pas. Rien ne doit etre dessine dans cet etat.
				bool IsSuspended() const noexcept {
					return !mSurfaceAlive;
				}

			protected:
				// --- A remplir par l'application ------------------------------

				/// Appele AVANT toute creation de fenetre. Rendre une valeur =
				/// le programme s'arrete la avec ce code de sortie.
				///
				/// C'est ce qui garde un banc lancable partout : pas de fenetre,
				/// pas de GPU, pas d'ecran. Le code de sortie EST le verdict.
				virtual NkOptional<int> OnCommandLine(const NkVector<NkString> &args) {
					(void)args;
					return NkOptional<int>();
				}

				/// Fenetre et cible de rendu existent. Rendre false = abandon.
				virtual bool OnInit() {
					return true;
				}

				virtual void OnUpdate(float32 deltaTime) {
					(void)deltaTime;
				}
				virtual void OnRender(NkRenderWindow &target) {
					(void)target;
				}
				virtual void OnShutdown() {
				}

				/// Le flux brut. true = consomme, la chaine s'arrete ici.
				/// Rendre false laisse la coquille faire son travail typé.
				virtual bool OnEvent(const NkEvent &event) {
					(void)event;
					return false;
				}

				/// Geste unifie souris + doigt. Derive de OnEvent, jamais d'un
				/// second abonnement.
				virtual bool OnPointer(const NkPointer &pointer) {
					(void)pointer;
					return false;
				}

				virtual bool OnKeyPress(const NkKeyPressEvent &event) {
					(void)event;
					return false;
				}

				/// Taille, zone sure et densite ont change (redimensionnement,
				/// rotation, retour d'arriere-plan). Appele AU MOINS une fois
				/// avant la premiere trame — jamais besoin de tester "est-ce
				/// que j'ai deja recu ma mise en page ?".
				virtual void OnLayout(const NkLayoutInfo &layout) {
					(void)layout;
				}

				/// DEMANDE de fermeture : croix de la fenetre, Alt+F4, geste
				/// systeme. Rendre false REFUSE la fermeture et rend la main a
				/// l'application.
				///
				/// C'est ce qui permet de poser une confirmation, de sauvegarder,
				/// ou de fermer plus tard soi-meme par Quit(). Sans ce point de
				/// refus, une coquille ferme toujours, et aucune application ne
				/// peut proteger un travail non enregistre.
				///
				/// ⚠️ Le defaut est `true` — on ferme. Un defaut a `false`
				/// rendrait injouable toute application qui oublie de le
				/// redefinir : elle refuserait de se fermer, et la seule issue
				/// serait de tuer le processus.
				///
				///     bool OnCloseRequested() override {
				///         if (mTravailEnregistre) return true;
				///         mDialogueQuitter = true;   // on affiche, on ne ferme pas
				///         return false;
				///     }
				virtual bool OnCloseRequested() {
					return true;
				}

				/// L'application part en arriere-plan ou perd le focus.
				/// C'est ICI que l'on coupe le son : sans cela, il continue de
				/// jouer par-dessus ce que l'utilisateur fait ensuite.
				virtual void OnPause() {
				}
				virtual void OnResume() {
				}

				// =====================================================
				// CE QUE LE SYSTEME NE LIVRE PAS, IL ACCEPTE DE LE FAIRE
				// =====================================================
				// Aucune plateforme mobile ne livre a une application les
				// touches ACCUEIL, APPLICATIONS RECENTES, POWER, VEILLE.
				// Les trois methodes ci-dessous obtiennent le RESULTAT que
				// l'on cherchait en les demandant -- par une autre porte.
				//
				// ⚠️ Elles rendent `false` quand la cible ne sait pas faire,
				// et le journalisent. Une methode qui rendrait `true` sans
				// rien faire serait pire qu'absente : l'appelant la croirait.

				/// Empeche l'ecran de s'eteindre. A REMETTRE A `false` en
				/// quittant la partie : sinon on vide la batterie d'un
				/// utilisateur qui a repose son telephone.
				bool GarderEcranAllume(bool actif);

				/// Epingle l'ecran : accueil et applications recentes cessent
				/// de sortir de l'application. C'est la reponse reelle a
				/// « je veux ces boutons » -- non pas les recevoir, mais les
				/// neutraliser.
				///
				/// ⚠️ Le systeme laisse une sortie deliberee (appui long sur
				/// Retour + Recentes) hors mode proprietaire d'appareil. Une
				/// application qui pourrait sequestrer un telephone serait une
				/// arme : on documente la limite au lieu de la contourner.
				/// Usage prevu : mode enfant, demonstration, examen.
				bool EpinglerEcran(bool actif);

				/// Pourquoi l'application a perdu le premier plan.
				enum class Raison {
					ECRAN_ETEINT,		 ///< bouton power, ou veille automatique
					PREMIER_PLAN_PERDU,	 ///< accueil, recentes, ou une autre application
					INCONNUE			 ///< la question n'a pas pu etre posee
				};

				/// ⚠️ NE SEPARE PAS ACCUEIL DE RECENTES : aucune API publique
				/// ne le dit, donc on ne l'invente pas. Rendre « probablement
				/// accueil » serait une supposition que l'appelant croirait.
				Raison RaisonDeLaPause() const {
					return mRaisonPause;
				}


			private:
				int Execute(const NkEntryState &state);
				bool CreateWindowAndTarget();
				NkGraphicsApi ResoudreBackend(const NkVector<NkString> &args) const;
				void PumpEvents();
				/// Interroge la cible AU MOMENT de la pause : dix secondes plus
				/// tard, l'ecran a pu s'eteindre pour une autre raison.
				void MesurerRaisonDeLaPause();
				bool HandleLifecycle(const NkEvent &event);
				void DispatchTyped(const NkEvent &event);
				void RefreshLayout(bool force);
				/// Plafonne la cadence ET rend la main. Voir le .cpp : les deux
				/// branches cedent, et c'est ce qui evite le gel de l'onglet Web.
				void CadencerTrame(NkChrono &chrono);

				NkCanvasAppConfig mConfig;
				NkWindow mWindow;
				NkRenderWindow *mTarget = nullptr;
				NkGraphicsApi mBackendResolu = NkGraphicsApi::NK_GFX_API_NONE;

				// --capture=<fichier> [--capture-frame=N] : ecrit une image et sort.
				// ⚠️ CE N'EST PAS UN GADGET. Sans lui, la seule facon de savoir ce
				// qu'une application AFFICHE est de la lancer et de regarder — donc
				// impossible en integration continue, impossible sur une plateforme
				// qu'on n'a pas sous la main, et impossible a comparer d'une version
				// a l'autre. Un banc dit que les regles sont justes ; lui seul dit
				// que l'ecran l'est.
				NkString mCapturePath;
				int32 mCaptureFrame = 30;
				int32 mFrameIndex = 0;
				NkLayoutInfo mLayout;

				float32 mTime = 0.f;
				bool mRunning = true;
				bool mSurfaceAlive = true; ///< false = arriere-plan : pas de surface native
				bool mHasFocus = true;
				bool mPaused = false; ///< etat courant vu par OnPause/OnResume
				Raison mRaisonPause = Raison::INCONNUE; ///< renseigne a chaque OnPause
				uint32 mLastWidth = 0;
				uint32 mLastHeight = 0;
		};

	} // namespace renderer
} // namespace nkentseu
