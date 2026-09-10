// =============================================================================
// NkCanvasApp.cpp — la boucle, et rien d'autre
//
// PROVENANCE : ce fichier n'invente rien. Il est le code eprouve de
// Applications/Gemcrush/src/main.cpp (lignes ~1000 a ~1130), qui tourne sur les
// sept plateformes, ecrit UNE fois au lieu de quatorze. Le patron du cycle de
// vie mobile vient lui-meme de Mou, qui l'avait paye avant.
//
// CE QUI EST DELICAT ICI, ET QUI NE SE DEVINE PAS EN LISANT
//   1. En arriere-plan on continue de VIDER la file d'evenements — c'est elle
//      qui apportera NkWindowShownEvent. Cesser de depiler, c'est ne jamais
//      apprendre qu'on est revenu.
//   2. On recree la surface AVANT de reprendre le rendu. Une trame de plus dans
//      l'ancienne surface part dans le vide, sans la moindre erreur GL.
//   3. Au retour, on force le recalcul de la mise en page : la taille a pu
//      changer pendant l'absence (rotation en arriere-plan) et AUCUN evenement
//      de redimensionnement ne viendra le dire.
// =============================================================================
#include "NKCanvas/App/NkCanvasApp.h"

#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKLogger/NkLog.h"
#include "NKPlatform/NkEnv.h"
#include "NKMemory/NKMemory.h"
#include "NKTime/NkChrono.h"
#include "NKTime/NkClock.h"

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
// Ce que le systeme ne livre pas en touches, il accepte de le faire : voir
// NkAndroidWindow.h. L'include est sous garde -- il n'existe que la.
// ⚠️ PAS `NKWindow/EntryPoints/NkAndroid.h` : cet en-tete DEFINIT
// `android_main`. L'inclure ici en produit un second exemplaire et le
// lien echoue sur « duplicate symbol » -- un en-tete de point d'entree ne
// s'inclut qu'une fois, dans le fichier qui EST le point d'entree.
// On declare donc le seul symbole dont on a besoin.
#include "NKWindow/Platform/Android/NkAndroidWindow.h"
struct android_app;
namespace nkentseu {
	extern android_app *nk_android_global_app;
}
#endif

namespace nkentseu {
	namespace renderer {

		// =====================================================================
		// Execute — l'ordre des etapes est le livrable, pas un detail
		// =====================================================================
		int NkCanvasApp::Execute(const NkEntryState &state) {

			// -- 0. AVANT TOUTE FENETRE ----------------------------------
			// Un banc, une aide, une conversion de fichier : tout ce qui peut
			// repondre sans ecran repond ici, et le programme s'arrete. C'est
			// ce qui rend une application testable sur une machine sans GPU.
			{
				NkOptional<int> early = OnCommandLine(state.args);
				if (early.HasValue()) {
					return early.Value();
				}
			}

			// Le backend se resout AVANT la fenetre, et se journalise.
			mBackendResolu = ResoudreBackend(state.args);

			// --capture / --capture-frame : offerts a TOUTE application de la
			// coquille, sans qu'elle ait une ligne a ecrire.
			for (uint32 i = 0; i < state.args.Size(); ++i) {
				const NkString &a = state.args[i];
				if (a.StartsWith("--capture=")) {
					mCapturePath = a.SubStr(10);
				} else if (a.StartsWith("--capture-frame=")) {
					mCaptureFrame = NkString(a.SubStr(16)).ToInt32();
				} else if (a.StartsWith("--taille=")) {
					// --taille=LxH : impose la taille de la fenetre.
					//
					// ⚠️ SANS ELLE, ON NE PEUT PAS REGARDER UNE MISE EN PAGE EN
					// PAYSAGE SUR UN BUREAU. Le 2026-09-03, Rodolf a signale
					// qu'apres rotation « ce n'est pas proportionnel et ca laisse
					// du vide » : le banc pouvait chiffrer les rectangles, mais
					// aucune capture ne pouvait MONTRER l'ecran tourne, faute de
					// pouvoir donner a la fenetre la forme d'un telephone couche.
					//
					// 📌 `--capture` sans `--taille` ne prouve que la mise en page
					// PAR DEFAUT. Les deux vont ensemble.
					const NkString v = a.SubStr(9);
					const int32 x = v.Find("x");
					if (x > 0) {
						const int32 l = NkString(v.SubStr(0, static_cast<uint32>(x))).ToInt32();
						const int32 h = NkString(v.SubStr(static_cast<uint32>(x) + 1)).ToInt32();
						if (l > 0 && h > 0) {
							mConfig.width = static_cast<uint32>(l);
							mConfig.height = static_cast<uint32>(h);
						} else {
							logger.Warnf("[nkcanvasapp] --taille=%s ignoree : L et H doivent etre > 0\n", v.CStr());
						}
					} else {
						logger.Warnf("[nkcanvasapp] --taille=%s ignoree : forme attendue LxH (ex. 2400x1080)\n",
									 v.CStr());
					}
				}
			}

			if (!CreateWindowAndTarget()) {
				return -1;
			}

			// La mise en page est calculee AVANT OnInit : une application ne
			// doit jamais avoir a tester "ai-je deja recu ma taille ?".
			RefreshLayout(true);

			if (!OnInit()) {
				logger.Error("[nkcanvasapp] OnInit a refuse de demarrer");
				if (mTarget != nullptr) {
					memory::NkGetDefaultAllocator().Delete(mTarget);
					mTarget = nullptr;
				}
				mWindow.Close();
				return -2;
			}

			// -- Cycle de vie : abonnements ------------------------------
			// Ils sont poses APRES OnInit pour que l'application puisse poser
			// les siens en premier — l'ordre d'appel des callbacks suit
			// l'ordre d'abonnement.
			auto &events = NkEvents();
			// ⚠️ PAS de callback sur NkWindowCloseEvent ici. Il en existait un,
			// et il creait DEUX chemins pour la meme demande : le callback part
			// a l'empilement, donc il fermait AVANT que HandleLifecycle ait pu
			// consulter l'application. Une application qui refuse la fermeture
			// aurait vu sa fenetre disparaitre quand meme.
			// La fermeture passe donc par HandleLifecycle, et par lui seul.

			// ARRIERE-PLAN — Android, iOS et HarmonyOS partagent le MEME cycle
			// de vie de surface : la fenetre native appartient au systeme, elle
			// est DETRUITE au depart en arriere-plan et RECREEE au retour.
			//
			// ⚠️ Sans ce bloc, l'application revient sur un ECRAN NOIR : elle
			// rend a pleine vitesse dans une surface que le compositeur
			// n'affiche plus, et pas une seule erreur n'est emise. C'est le
			// defaut que la moitie des applications du depot portent encore.
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_HARMONYOS) || \
	defined(NKENTSEU_PLATFORM_IOS)
			events.AddEventCallback<NkWindowHiddenEvent>([this](NkWindowHiddenEvent *) {
				mSurfaceAlive = false;
				if (!mPaused) {
					mPaused = true;
					MesurerRaisonDeLaPause();
					OnPause();
				}
				logger.Info("[nkcanvasapp] arriere-plan : rendu suspendu");
			});
			events.AddEventCallback<NkWindowShownEvent>([this](NkWindowShownEvent *) {
				// Recreer la surface AVANT de reprendre le rendu.
				const bool recreated = (mTarget != nullptr) ? mTarget->RecreateSurface() : false;
				mSurfaceAlive = true;
				// La taille a pu changer pendant l'absence : on force le
				// recalcul plutot que d'attendre un evenement qui ne viendra pas.
				mLastWidth = 0;
				mLastHeight = 0;
				if (mPaused) {
					mPaused = false;
					OnResume();
				}
				logger.Info("[nkcanvasapp] retour au premier plan : surface recreee = {0}", recreated);
			});
#endif

			// PERTE DE FOCUS — toutes plateformes. La fenetre reste visible mais
			// l'application n'a plus la main. Sur mobile c'est ce qui arrive
			// quand une notification ou un appel passe devant.
			events.AddEventCallback<NkWindowFocusLostEvent>([this](NkWindowFocusLostEvent *) {
				mHasFocus = false;
				if (!mPaused) {
					mPaused = true;
					MesurerRaisonDeLaPause();
					OnPause();
				}
			});
			events.AddEventCallback<NkWindowFocusGainedEvent>([this](NkWindowFocusGainedEvent *) {
				mHasFocus = true;
				if (mPaused && mSurfaceAlive) {
					mPaused = false;
					OnResume();
				}
			});

			// -- Boucle ---------------------------------------------------
			NkClock clock;
			NkChrono chronoTrame;
			while (mRunning && mWindow.IsOpen()) {
				(void)chronoTrame.Reset(); // depart de la trame, pour la cadence

				float32 deltaTime = clock.Tick().delta;
				if (deltaTime > mConfig.maxDeltaTime) {
					deltaTime = 1.f / 60.f; // reprise apres veille : on ne rattrape pas
				}

				// ARRIERE-PLAN : aucun rendu, mais on continue de depiler —
				// c'est la file qui apportera NkWindowShownEvent.
				if (!mSurfaceAlive) {
					PumpEvents();
					NkChrono::Sleep(static_cast<int64>(32));
					continue;
				}

				RefreshLayout(false);
				PumpEvents();
				if (!mRunning) {
					break;
				}

				mTime += deltaTime;
				OnUpdate(deltaTime);

				mTarget->Clear(mConfig.clearColor);
				OnRender(*mTarget);
				mTarget->Display();

				CadencerTrame(chronoTrame);

				// La capture se prend APRES Display() : avant, le tampon n'a pas
				// encore ete presente et l'image serait celle de la trame
				// precedente — un decalage d'une trame qui ne se voit pas sur une
				// image fixe et fausse tout ce qui bouge.
				++mFrameIndex;
				if (mCapturePath.Size() > 0 && mFrameIndex >= mCaptureFrame) {
					const bool ok = mTarget->Capture(mCapturePath.Data());
					logger.Info("[nkcanvasapp] capture vers {0} : {1}", mCapturePath.Data(), ok ? "ok" : "ECHEC");
					mRunning = false;
				}
			}

			OnShutdown();
			if (mTarget != nullptr) {
				memory::NkGetDefaultAllocator().Delete(mTarget);
				mTarget = nullptr;
			}
			mWindow.Close();
			return 0;
		}

		// =====================================================================
		// ResoudreBackend — la directive du 2026-08-18, appliquee UNE fois
		//
		// "Pour toutes nos applications, on doit pouvoir choisir le backend
		//  graphique entre ceux disponibles."
		//
		// Quatre sources, de la plus locale a la plus durable :
		//   1. --backend=<nom>  ce lancement
		//   2. NK_GFX_BACKEND   cette session
		//   3. Config().backend le defaut de l'application
		//   4. defaut de plateforme
		//
		// ⚠️ ET LE JOURNAL DIT LEQUEL DES QUATRE A DECIDE. Sans cette ligne, un
		// utilisateur qui regle une valeur et en voit une autre se lancer n'a
		// aucun moyen de comprendre — et un agent passe des heures a croire
		// qu'il mesure Vulkan alors qu'il mesure OpenGL. C'est arrive.
		// =====================================================================
		NkGraphicsApi NkCanvasApp::ResoudreBackend(const NkVector<NkString> &args) const {

			auto depuisTexte = [](const char *t) -> NkGraphicsApi {
				if (t == nullptr) {
					return NkGraphicsApi::NK_GFX_API_NONE;
				}
				const NkString v(t);
				if (v == "opengl" || v == "gl") {
					return NkGraphicsApi::NK_GFX_API_OPENGL;
				}
				if (v == "vulkan" || v == "vk") {
					return NkGraphicsApi::NK_GFX_API_VULKAN;
				}
				if (v == "dx11" || v == "d3d11") {
					return NkGraphicsApi::NK_GFX_API_DX11;
				}
				if (v == "dx12" || v == "d3d12") {
					return NkGraphicsApi::NK_GFX_API_DX12;
				}
				if (v == "metal") {
					return NkGraphicsApi::NK_GFX_API_METAL;
				}
				if (v == "software" || v == "sw") {
					return NkGraphicsApi::NK_GFX_API_SOFTWARE;
				}
				return NkGraphicsApi::NK_GFX_API_NONE;
			};

			// -- 1. Ligne de commande ------------------------------------
			for (uint32 i = 0; i < args.Size(); ++i) {
				const NkString &a = args[i];
				if (a.StartsWith("--backend=")) {
					const NkString nom = a.SubStr(10);
					const NkGraphicsApi api = depuisTexte(nom.Data());
					if (api != NkGraphicsApi::NK_GFX_API_NONE) {
						logger.Info("[nkcanvasapp] backend demande par --backend={0}", nom.Data());
						return api;
					}
					// ⚠️ Un nom inconnu se DIT. Le remplacer en silence par le
					// defaut ferait mesurer autre chose que ce qui est demande.
					logger.Warn("[nkcanvasapp] backend inconnu dans --backend={0} : on poursuit avec le defaut",
								nom.Data());
				}
			}

			// -- 2. Environnement ----------------------------------------
			// ⚠️ GetEnvVar rend un const char* qui vaut nullptr quand la
			// variable est absente. On le teste AVANT de le convertir : une
			// NkString construite depuis nullptr donne une chaine vide, et
			// "absente" deviendrait indistinguable de "definie a vide".
			{
				const char *brut = env::GetEnvVar("NK_GFX_BACKEND");
				if (brut != nullptr && brut[0] != '\0') {
					const NkGraphicsApi api = depuisTexte(brut);
					if (api != NkGraphicsApi::NK_GFX_API_NONE) {
						logger.Info("[nkcanvasapp] backend demande par NK_GFX_BACKEND={0}", brut);
						return api;
					}
					logger.Warn("[nkcanvasapp] NK_GFX_BACKEND={0} inconnu : on poursuit avec le defaut", brut);
				}
			}

			// -- 3. Defaut de l'application ------------------------------
			if (mConfig.backend != NkGraphicsApi::NK_GFX_API_NONE) {
				logger.Info("[nkcanvasapp] backend impose par l'application = {0}",
							NkGraphicsApiName(mConfig.backend));
				return mConfig.backend;
			}

			// -- 4. Defaut de plateforme ---------------------------------
			// Ce ne sont pas des preferences : ce sont les backends reellement
			// cables et eprouves sur chaque plateforme.
#if defined(NKENTSEU_PLATFORM_WINDOWS)
			const NkGraphicsApi defaut = NkGraphicsApi::NK_GFX_API_DX11;
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_HARMONYOS) || \
	defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_ENABLE_EMSCRIPTEN)
			const NkGraphicsApi defaut = NkGraphicsApi::NK_GFX_API_OPENGLES;
#else
			const NkGraphicsApi defaut = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
			logger.Info("[nkcanvasapp] backend par defaut de la plateforme = {0}", NkGraphicsApiName(defaut));
			return defaut;
		}

		// =====================================================================
		// CadencerTrame — plafonner la cadence ET CEDER LA MAIN
		//
		// ⚠️ LES DEUX BRANCHES CEDENT, ET C'EST LE POINT.
		//   en avance -> Sleep(reste)   -> emscripten_sleep(ms) sur le Web
		//   en retard  -> YieldThread() -> emscripten_sleep(0)  sur le Web
		// Une version « ne dormir que si on est en avance » gele l'onglet des la
		// premiere trame lente — c'est-a-dire tout le temps sur le Web, ou une
		// trame depasse presque toujours le budget. Le `else` n'est pas une
		// optimisation : c'est lui qui rend la main.
		//
		// Hors du Web, l'effet est celui qu'on attend d'un plafond : on rend le
		// processeur au systeme au lieu de bruler une trame a vide.
		// =====================================================================
		void NkCanvasApp::CadencerTrame(NkChrono &chrono) {
			if (mConfig.imagesParSeconde <= 0) {
				// Sans plafond : on cede quand meme. Sur le Web c'est
				// obligatoire ; ailleurs c'est gratuit et poli.
				NkChrono::YieldThread();
				return;
			}
			const float64 budgetMs = 1000.0 / static_cast<float64>(mConfig.imagesParSeconde);
			const float64 ecouleMs = chrono.Elapsed().milliseconds;
			if (ecouleMs < budgetMs) {
				NkChrono::Sleep(budgetMs - ecouleMs);
			} else {
				NkChrono::YieldThread();
			}
		}

		// =====================================================================
		// CreateWindowAndTarget
		// =====================================================================
		bool NkCanvasApp::CreateWindowAndTarget() {
			NkWindowConfig cfg;
			cfg.title = mConfig.title;
			cfg.width = mConfig.width;
			cfg.height = mConfig.height;
			cfg.centered = mConfig.centered;
			cfg.resizable = mConfig.resizable;
			cfg.frame = mConfig.frame;

			if (!mWindow.Create(cfg)) {
				logger.Error("[nkcanvasapp] creation de fenetre impossible");
				return false;
			}

			NkContextDesc desc;
			desc.api = mBackendResolu;

			// ⚠️ Alloue par NKMemory, jamais par new : melanger l'allocateur
			// maison et le tas CRT corrompt le tas sous Windows (c0000374).
			mTarget = memory::NkGetDefaultAllocator().New<NkRenderWindow>(mWindow, desc);
			if (mTarget == nullptr || !mTarget->IsValid()) {
				logger.Error("[nkcanvasapp] initialisation de NkRenderWindow ECHOUEE");
				if (mTarget != nullptr) {
					memory::NkGetDefaultAllocator().Delete(mTarget);
					mTarget = nullptr;
				}
				mWindow.Close();
				return false;
			}

			// Le backend RETENU, pas celui demande : quand les deux different,
			// c'est la seule ligne qui permet de le savoir.
			logger.Info("[nkcanvasapp] {0} — backend graphique retenu = {1}", mConfig.title, NkGraphicsApiName(desc.api));
			return true;
		}

		// =====================================================================
		// RefreshLayout — appele a chaque trame, ne fait rien si rien n'a bouge
		// =====================================================================
		void NkCanvasApp::RefreshLayout(bool force) {
			if (mTarget == nullptr) {
				return;
			}
			const math::NkVec2u size = mTarget->GetSize();
			if (!force && size.x == mLastWidth && size.y == mLastHeight) {
				return;
			}

			// mLastWidth == 0 signifie "premiere mesure" : la cible est deja a
			// la bonne taille, la redimensionner serait un aller-retour inutile
			// qui detruit et recree les ressources de swapchain.
			if (!force && mLastWidth != 0 && size.x > 0 && size.y > 0) {
				mTarget->OnResize(size.x, size.y);
			}
			mLastWidth = size.x;
			mLastHeight = size.y;

			mLayout.width = size.x;
			mLayout.height = size.y;
			mLayout.density = mWindow.GetDpiScale();
			if (mLayout.density <= 0.f) {
				mLayout.density = 1.f;
			}
			mLayout.safeArea = mWindow.GetSafeAreaInsets();
			OnLayout(mLayout);
		}

		// =====================================================================
		// PumpEvents — UN chemin, deux sorties
		// =====================================================================
		void NkCanvasApp::PumpEvents() {
			auto &events = NkEvents();
			while (NkEvent *event = events.PollEvent()) {
				if (event == nullptr || !mRunning) {
					break;
				}
				// 1. Ce que personne ne peut faire a la place de la coquille.
				if (HandleLifecycle(*event)) {
					continue;
				}
				// 2. L'application, en brut. true = consomme.
				if (OnEvent(*event)) {
					continue;
				}
				// 3. Les virtuelles typees, DERIVEES du meme evenement.
				DispatchTyped(*event);
			}
		}

		// =====================================================================
		// HandleLifecycle — la coquille ne consomme QUE ce qui la regarde
		// =====================================================================
		bool NkCanvasApp::HandleLifecycle(const NkEvent &event) {
			if (event.As<NkWindowCloseEvent>() != nullptr) {
				// On DEMANDE, on n'impose pas. Une application peut avoir un
				// travail non enregistre, une partie en cours, une confirmation
				// a poser : rendre false laisse la fenetre ouverte et lui rend
				// la main. Elle fermera quand elle voudra, par Quit().
				if (OnCloseRequested()) {
					mRunning = false;
				}
				// Consomme dans les DEUX cas : la demande a ete traitee, qu'elle
				// soit acceptee ou refusee. La laisser passer ferait croire a un
				// evenement non traite.
				return true;
			}
			// Le redimensionnement n'est PAS consomme : RefreshLayout l'a deja
			// vu par la taille de la cible, et une application peut vouloir
			// reagir a l'evenement lui-meme.
			return false;
		}

		// =====================================================================
		// DispatchTyped — le SEUL endroit ou naissent les virtuelles typees
		//
		// ⚠️ Ajouter un evenement typé se fait ICI, jamais par un
		// AddEventCallback separe. Deux routes vers la meme virtuelle, et plus
		// personne ne peut dire laquelle a repondu.
		// =====================================================================
		void NkCanvasApp::DispatchTyped(const NkEvent &event) {
			NkPointer pointer;
			if (NkReadPointer(event, pointer)) {
				if (OnPointer(pointer)) {
					return;
				}
			}
			if (const auto *key = event.As<NkKeyPressEvent>()) {
				if (OnKeyPress(*key)) {
					return;
				}
#if defined(NKENTSEU_PLATFORM_ANDROID)
				// ── RETOUR NON CONSOMME = ON QUITTE, COMME TOUTE APPLICATION ─
				// Sur Android, AKEYCODE_BACK arrive ici en NK_ESCAPE, et le
				// backend natif a DEJA repondu « traite » au systeme (il ne peut
				// pas attendre : l'evenement est mis en file). Le systeme ne
				// fermera donc jamais l'activite a notre place. Les jeux ecrivaient
				// « au menu, on laisse le systeme fermer » -- et rien ne fermait :
				// la touche retour etait muette au menu. C'est la coquille qui
				// tient le contrat de la plateforme : un retour que personne ne
				// reclame quitte l'application.
				//
				// Bureau exclu volontairement : Echap n'y ferme pas une fenetre,
				// et une coquille qui le ferait surprendrait toutes ses
				// applications d'un coup.
				if (NkEstToucheRetour(key->GetKey())) {
					mRunning = false;
				}
#endif
			}
		}

		// =====================================================================
		// LES TROIS CAPACITES, ET CE QU'ELLES DISENT QUAND ELLES NE PEUVENT PAS
		// =====================================================================
		// Aucune plateforme mobile ne livre a une application les touches
		// ACCUEIL, APPLICATIONS RECENTES, POWER, VEILLE. Ces trois methodes
		// obtiennent le RESULTAT qu'on cherchait en les demandant, par une
		// autre porte : empecher la veille, bloquer accueil et recentes,
		// savoir pourquoi on a perdu le premier plan.
		//
		// ⚠️ LE `#if` EST ICI, DANS LE MOTEUR, ET NULLE PART AILLEURS. Une
		// application qui ecrirait `si (android)` aurait deja perdu : la
		// decision aurait fui vers son client et s'y recopierait autant de
		// fois qu'il y a de clients.

		bool NkCanvasApp::GarderEcranAllume(bool actif) {
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
			return NkAndroidGarderEcranAllume(nk_android_global_app, actif);
#else
			// Pas un silence : un refus qui se voit. Le jour ou quelqu'un
			// cherchera pourquoi son ecran s'eteint sur cette cible, la ligne
			// sera dans le journal. Un `return true` qui ne fait rien serait un
			// parametre non honore -- l'appelant le croirait.
			logger.Warnf("[nkcanvasapp] garder l'ecran allume : non implemente ici (demande : %s)\n",
						 actif ? "oui" : "non");
			return false;
#endif
		}

		bool NkCanvasApp::EpinglerEcran(bool actif) {
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
			return NkAndroidEpinglerEcran(nk_android_global_app, actif);
#else
			logger.Warnf("[nkcanvasapp] epinglage d'ecran : non implemente ici (demande : %s)\n",
						 actif ? "oui" : "non");
			return false;
#endif
		}

		// La raison se mesure AU MOMENT de la pause, pas apres : dix secondes
		// plus tard, l'ecran a pu s'eteindre pour une tout autre raison, et la
		// reponse ne dirait plus pourquoi on est parti.
		void NkCanvasApp::MesurerRaisonDeLaPause() {
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
			bool horsService = true;
			const bool allume = NkAndroidEcranAllume(nk_android_global_app, &horsService);
			// ⚠️ « je n'ai pas pu demander » n'est pas « l'ecran est allume ».
			// Les confondre ferait passer un echec de mesure pour un fait.
			mRaisonPause = horsService ? Raison::INCONNUE
									   : (allume ? Raison::PREMIER_PLAN_PERDU : Raison::ECRAN_ETEINT);
#else
			mRaisonPause = Raison::PREMIER_PLAN_PERDU;
#endif
		}

	} // namespace renderer
} // namespace nkentseu
