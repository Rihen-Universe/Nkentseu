// =============================================================================
// Nogee/Shell/NogeeShell.cpp — coquille d'editeur optionnelle (cf. .h)
// =============================================================================
// Modele de montage : Applications/NkAnimaEditor/src/NkAnimaEditor/main.cpp.
// =============================================================================
#include "Nogee/Shell/NogeeShell.h"
#include "Nogee/Panels/ConsolePanelGui.h"
#include "Nogee/Panels/WorldOutlinerPanel.h"
#include "Nogee/Panels/DetailsPanel.h"
#include "Nogee/Panels/ContentBrowserPanel.h"
#include "Nogee/Editor/NkSelectionManager.h"
#include "Nogee/Editor/CommandHistory.h"
#include "Nogee/Editor/AssetManager.h"
#include "Nogee/Editor/ProjectManager.h"

#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"

#include "NKEditorKit/NkEditorKit.h"
#include "NKGui/NkEditorRHIRenderer.h" // Integrations/NKGui (impl generalisee)
#include "NKMemory/NkUniquePtr.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkentseu::editorkit;

		namespace {

			ConsolePanelGui g_console;
			NkEditorShell *g_shell = nullptr;

			// ── SONDE D'OCCULTATION ───────────────────────────────────────────
			// Pourquoi une sonde et pas un clic : une mesure doit etre
			// reproductible, et personne ne peut cliquer dans un run automatise.
			// On interroge donc le routeur LUI-MEME, a l'execution, avec la
			// geometrie REELLE de la palette — ce qui est plus proche du defaut
			// que de simuler un evenement souris.
			//
			// Le controle compte autant que le cas : sans un point qui repond
			// VRAI, un « faux » ne prouverait pas l'occultation, seulement que la
			// sonde ne repond jamais oui.
			struct OcclusionProbe {
					bool enabled = false;
					int32 frame = 0;
					bool paletteOpened = false;
					bool testPrefs = false; ///< mesurer les Preferences au lieu de la palette
					/// Reproduit la condition de ConquerorLab (main.cpp:219) :
					/// SetMaskBodyOnPopup(false). Sert a mesurer si ce drapeau
					/// neutralise ou non le correctif d'occlusion de la palette.
					bool noMaskBody = false;
					bool hoverableNoVeil = false;
					bool panelNoVeil = false;
					bool reported = false;
			};

			OcclusionProbe g_probe;

			// ── PANNEAU-SONDE ─────────────────────────────────────────────────
			// La sonde d'overlay s'est revelee NON CONCLUANTE : son temoin
			// (meme mesure, palette fermee) rendait deja 0, donc elle mesurait
			// son propre point de vue et non la palette. Un panneau reel a le
			// bon clip, la bonne fenetre courante : c'est de LA qu'il faut
			// interroger la porte complete.
			class ProbePanel final : public NkEditorPanel {
				public:
					ProbePanel() noexcept : NkEditorPanel("Sonde", NkEditorDockSide::NK_BOTTOM) {
					}

					void OnUI(NkEditorFrameContext &ec) override {
						nkgui::NkGuiContext &ui = ec.Ui();
						ec.Text("panneau-sonde (occultation)");
						// Rect REEL de ce panneau, obtenu du curseur courant.
						const nkgui::NkRect r = ui.NextItemRect(120.f, 20.f);
						const nkgui::NkVec2 c = {r.x + r.w * 0.5f, r.y + r.h * 0.5f};
						// ⚠️ CE QU'IL FAUT RELEVER AVANT DE FORCER QUOI QUE CE SOIT :
						// la souris que le PANNEAU recoit reellement. Le shell
						// BLANCHIT l'entree du corps (mousePos = -100000) quand il
						// se juge « modal » — et `modal` contient mShowPrefs, mais
						// PAS la palette (NkEditorShell.cpp:693). Forcer mousePos
						// sans relever cette valeur revient a defaire la protection
						// qu'on croit mesurer.
						mIncomingMouseX = ui.input.mousePos.x;

						const nkgui::NkVec2 saved = ui.input.mousePos;
						ui.input.mousePos = c;
						mLastHoverable = ui.ItemHoverable(r, 0xC0FFEEu);
						ui.input.mousePos = saved;
						mHasMeasure = true;
					}

					bool mLastHoverable = false;
					float32 mIncomingMouseX = 0.f; ///< souris RECUE par le panneau, avant forcage
					bool mHasMeasure = false;
			};

			ProbePanel g_probePanel;

			void ProbeOverlay(NkEditorFrameContext &ec, void *) {
				if (!g_probe.enabled || g_probe.reported)
					return;
				nkgui::NkGuiContext &ui = ec.Ui();
				++g_probe.frame;

				// Phase 1 (frame 40) : palette FERMEE — c'est le TEMOIN de la
				// mesure de la phase 3. Sans lui, un `ItemHoverable=0` avec la
				// palette ouverte ne prouverait rien : il pourrait venir du clip
				// courant de l'overlay, pas de la palette. La meme mesure,
				// repetee sans rien changer d'autre, est la seule chose qui
				// separe les deux causes.
				if (g_probe.frame == 40) {
					logger.Info("[SONDE] phase 1 ({0} FERMEE) : occlCount={1} curInputLayer={2}\n",
								g_probe.testPrefs ? "PREFERENCES" : "palette", ui.occlCount, ui.curInputLayer);
					for (int32 i = 0; i < ui.occlCount; ++i)
						logger.Info("[SONDE]   rect declare : layer={0}\n", ui.occlLayers[i]);

					const nkgui::NkVec2 savedMouse = ui.input.mousePos;
					const nkgui::NkVec2 under = {static_cast<float32>(ui.viewW) * 0.5f,
												 static_cast<float32>(ui.viewH) * 0.80f};
					ui.input.mousePos = under;
					const nkgui::NkRect probeRect = {under.x - 40.f, under.y - 10.f, 80.f, 20.f};
					const bool hoverableNoVeil = ui.ItemHoverable(probeRect, 0xDEADBEEFu);
					ui.input.mousePos = savedMouse;
					logger.Info("[SONDE]   (overlay — non concluant) sans voile ItemHoverable={0}\n",
								hoverableNoVeil ? 1 : 0);
					g_probe.hoverableNoVeil = hoverableNoVeil;

					// LE VRAI TEMOIN : la mesure prise depuis le PANNEAU-SONDE,
					// qui a le clip et la fenetre courante d'un vrai panneau.
					logger.Info("[SONDE]   TEMOIN PANNEAU sans voile : mesure_faite={0} ItemHoverable={1} souris_recue_x={2}\n",
								g_probePanel.mHasMeasure ? 1 : 0, g_probePanel.mLastHoverable ? 1 : 0,
								(int32)g_probePanel.mIncomingMouseX);
					g_probe.panelNoVeil = g_probePanel.mLastHoverable;
				}

				// Phase 2 (frame 45) : on ouvre LA surface a tester.
				// UNE seule par execution : les deux voiles sont plein ecran, les
				// ouvrir ensemble melangerait les deux mesures.
				if (g_probe.frame == 45 && g_shell && !g_probe.paletteOpened) {
					if (g_probe.testPrefs) {
						g_shell->OpenPreferences();
						logger.Info("[SONDE] phase 2 : OpenPreferences() appelee\n");
					} else {
						g_shell->OpenCommandPalette();
						logger.Info("[SONDE] phase 2 : OpenCommandPalette() appelee\n");
					}
					g_probe.paletteOpened = true;
				}

				// Phase 3 (frame 55) : la palette est ouverte depuis >=2 frames,
				// donc sa surface figure dans la liste LUE (frame precedente).
				if (g_probe.frame == 55) {
					logger.Info("[SONDE] phase 3 ({0} OUVERTE) : occlCount={1} curInputLayer={2}\n",
								g_probe.testPrefs ? "PREFERENCES" : "palette", ui.occlCount, ui.curInputLayer);

					const int32 savedLayer = ui.curInputLayer;
					ui.curInputLayer = 0; // on interroge du point de vue d'un PANNEAU

					bool anyCovered = false;
					for (int32 i = 0; i < ui.occlCount; ++i) {
						const nkgui::NkRect &r = ui.occlRects[i];
						const nkgui::NkVec2 center = {r.x + r.w * 0.5f, r.y + r.h * 0.5f};
						const bool reachable = ui.PointReachable(center);
						logger.Info("[SONDE]   CAS      layer={0} centre=({1},{2}) PointReachable={3}\n",
									ui.occlLayers[i], (int32)center.x, (int32)center.y, reachable ? 1 : 0);
						if (!reachable)
							anyCovered = true;
					}

					// TEMOIN : un point volontairement HORS de toute surface
					// declaree. S'il repond faux lui aussi, la sonde ne prouve
					// rien — elle refuserait tout.
					const nkgui::NkVec2 witness = {4.f, static_cast<float32>(ui.viewH) - 4.f};
					const bool witnessReachable = ui.PointReachable(witness);
					logger.Info("[SONDE]   TEMOIN   coin bas-gauche PointReachable={0} (doit valoir 1)\n",
								witnessReachable ? 1 : 0);

					// ── LA QUESTION QUI COMPTE ────────────────────────────────
					// PointReachable n'interroge QUE le routeur. Un widget passe
					// par ItemHoverable, qui a QUATRE autres portes (desactive,
					// popup, fenetre survolee, clip). On mesure donc la porte
					// complete, avec une souris placee sous le VOILE plein ecran
					// de la palette — la ou un panneau ancre se trouve.
					const nkgui::NkVec2 savedMouse = ui.input.mousePos;
					const float32 py = static_cast<float32>(ui.viewH) * 0.80f;
					const nkgui::NkVec2 under = {static_cast<float32>(ui.viewW) * 0.5f, py};
					ui.input.mousePos = under;
					const nkgui::NkRect probeRect = {under.x - 40.f, under.y - 10.f, 80.f, 20.f};
					const bool hoverableUnderVeil = ui.ItemHoverable(probeRect, 0xDEADBEEFu);
					ui.input.mousePos = savedMouse;
					logger.Info("[SONDE]   PORTE COMPLETE sous le voile ({0},{1}) ItemHoverable={2}\n",
								(int32)under.x, (int32)under.y, hoverableUnderVeil ? 1 : 0);

					ui.curInputLayer = savedLayer;

					logger.Info("[SONDE] ROUTEUR : occultation_active={0} temoin_routeur_ok={1}\n",
								anyCovered ? 1 : 0, witnessReachable ? 1 : 0);

					// Le verdict n'a de sens QUE compare a son temoin. Si la porte
					// bloque deja SANS voile, la sonde ne mesure pas la palette :
					// elle mesure son propre point de vue (clip de l'overlay).
					logger.Info("[SONDE]   (overlay — non concluant) sous voile ItemHoverable={0}\n",
								hoverableUnderVeil ? 1 : 0);

					// VERDICT — fonde sur le PANNEAU-SONDE, et compare a SON temoin.
					// Si le panneau ne repondait deja pas sans voile, la mesure ne
					// porte pas sur la palette et on ne conclut rien.
					const bool panelUnderVeil = g_probePanel.mLastHoverable;
					const char *verdict =
						(!g_probe.panelNoVeil)
							? "NON CONCLUANT — le panneau ne repondait deja pas SANS voile"
							: (panelUnderVeil ? "LE CLIC TRAVERSE LE VOILE" : "le voile bloque bien");
					// ⚠️ `souris_recue_x` decide si le verdict vaut quelque chose.
					// Tres negatif (-100000) = le shell a DEJA blanchi l'entree du
					// corps (NkEditorShell.cpp:693-700) : le forcage de la sonde
					// DEFAIT cette protection, et le verdict ne prouve rien.
					const bool inputBlanked = g_probePanel.mIncomingMouseX < -1000.f;
					logger.Info("[SONDE] VERDICT PANNEAU : sans_voile={0} sous_voile={1} souris_recue_x={2} -> {3}\n",
								g_probe.panelNoVeil ? 1 : 0, panelUnderVeil ? 1 : 0,
								(int32)g_probePanel.mIncomingMouseX,
								inputBlanked ? "VERDICT NUL — entree du corps DEJA blanchie par le shell" : verdict);
					g_probe.reported = true;

					if (g_shell)
						g_shell->RequestClose();
				}
			}

			void CmdQuit(void *u) {
				if (u)
					static_cast<NkEditorShell *>(u)->RequestClose();
			}

		} // namespace

		int RunNogeeEditorShell(NogeAppConfig &cfg) noexcept {
			auto shell = memory::NkMakeUnique<NkEditorShell>();
			if (!shell) {
				logger.Error("[Nogee/Shell] allocation du shell impossible\n");
				return 2;
			}

			// Backend de rendu NKRHI injecte (PAS NKCanvas) : l'UI NKGui et un
			// futur viewport 3D partageront ce device. Doit survivre au shell.
			static nkgui::NkEditorRHIRenderer rhi;

			NkEditorShellConfig scfg;
			scfg.title = "Noge Editor — coquille NKEditorKit (--ui=rhi)";
			scfg.width = 1600;
			scfg.height = 900;
			scfg.graphicsApi = NkEditorGfxApi::OpenGL;
			scfg.renderer = &rhi;

			if (!shell->Init(scfg)) {
				logger.Error("[Nogee/Shell] NkEditorShell::Init a echoue\n");
				return 2;
			}

			g_shell = shell.Get();

			// Le panneau PORTE (NKGui). Son jumeau NKUI reste intact et sert le
			// chemin par defaut ; les deux partagent NkConsoleModel.
			shell->AddPanel(&g_console);
			if (g_probe.enabled)
				shell->AddPanel(&g_probePanel);

			// ── MONDE ECS + SYSTEMES EDITEUR COTE SHELL (2026-08-17) ─────────
			// Le geste du commit 1d8a100f, rejoue ici : sans monde, les panneaux
			// portes compilent mais ne peuvent rien montrer. Les systemes que les
			// panneaux consomment (selection, historique, assets, projet) sont
			// TOUS constructibles seuls — verifie : aucun ne demande de Layer ni
			// de NkApplication. Ce qui vivait dans EditorLayer etait une
			// POSSESSION, pas une dependance.
			static ecs::NkWorld sWorld;
			static ecs::NkSceneGraph sScene(sWorld, "Scene");
			static NkSelectionManager sSel;
			static CommandHistory sHist;
			static AssetManager sAssets;
			static ProjectManager sProject;

			// Les memes entites TEMOIN que le chemin NKUI — et le meme
			// complement explicite : SpawnNode et NkGameObjectFactory posent des
			// composants DISJOINTS (cf. carnet), aucun des deux ne suffit.
			{
				const ecs::NkEntityId racine = sScene.SpawnNode("TEMOIN_Racine");
				const ecs::NkEntityId enfantA = sScene.SpawnNode("TEMOIN_Enfant_A");
				const ecs::NkEntityId enfantB = sScene.SpawnNode("TEMOIN_Enfant_B");
				sScene.SetParent(enfantA, racine);
				sScene.SetParent(enfantB, racine);
				const ecs::NkEntityId ids[3] = {racine, enfantA, enfantB};
				const char *noms[3] = {"TEMOIN_Racine", "TEMOIN_Enfant_A", "TEMOIN_Enfant_B"};
				for (int i = 0; i < 3; ++i) {
					sWorld.Add<ecs::NkName>(ids[i], ecs::NkName(noms[i]));
					sWorld.Add<ecs::NkTransform>(ids[i]);
				}
				logger.Info("[Nogee/Shell] Monde ECS : 3 entites TEMOIN_* creees\n");
			}

			// Assets + projet : racine = projet de demarrage s'il existe, sinon
			// le repertoire courant (defaut raisonnable, ANNONCE — la spec est
			// silencieuse sur la racine hors projet).
			const char *projectDir =
				cfg.startupProjectPath.Empty() ? "." : cfg.startupProjectPath.CStr();
			sAssets.Init(rhi.GetDevice(), projectDir);
			if (!cfg.startupProjectPath.Empty())
				sProject.Load(cfg.startupProjectPath.CStr());

			// ── Les 3 panneaux portes restants, lies puis enregistres ─────────
			static WorldOutlinerPanel sOutliner;
			static DetailsPanel sDetails;
			static ContentBrowserPanel sContent;

			sOutliner.Bind(&sWorld, &sScene, &sSel, &sHist);
			sDetails.Bind(&sWorld, &sSel, &sHist);
			sContent.Init(&sAssets, projectDir);

			shell->AddPanel(&sOutliner);
			shell->AddPanel(&sDetails);
			shell->AddPanel(&sContent);

			// Selection initiale : le Details Panel montre le TEMOIN au premier
			// rendu au lieu d'attendre un clic (c'est aussi ce qui rend le temoin
			// numerique verifiable sans souris).
			{
				NkVector<ecs::NkEntityId> roots;
				sWorld.Query<const ecs::NkSceneNode>().ForEach(
					[&](ecs::NkEntityId id, const ecs::NkSceneNode &n) {
						if (roots.Empty() && n.name[0] != '\0')
							roots.PushBack(id);
					});
				if (!roots.Empty())
					sSel.Select(roots[0]);
			}

			// De quoi remplir la console : sans lignes, un panneau vide ne
			// prouverait pas qu'il dessine.
			g_console.PushLine("Nogee — coquille NKEditorKit montee", NkLogLevel::NK_INFO);
			g_console.PushLine("panneau Console porte sur NKGui (pilote)", NkLogLevel::NK_INFO);
			g_console.PushLine("modele partage avec la version NKUI", NkLogLevel::NK_WARN);
			g_console.PushLine("ceci est une ligne d'erreur de demonstration", NkLogLevel::NK_ERROR);

			// Reproduction de la condition de ConquerorLab (main.cpp:219). Elle
			// permet de mesurer, dans MON banc, si ce drapeau neutralise le
			// correctif d'occlusion de la palette — au lieu d'en decider par
			// lecture des deux mecanismes.
			if (g_probe.noMaskBody) {
				shell->SetMaskBodyOnPopup(false);
				logger.Info("[SONDE] condition ConquerorLab reproduite : SetMaskBodyOnPopup(false)\n");
			}

			shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");
			shell->SetOverlay(&ProbeOverlay, nullptr);

			if (g_probe.enabled)
				logger.Info("[SONDE] activee : mesure du routeur d'occultation a l'execution\n");

			logger.Info("[Nogee/Shell] coquille prete — 4 panneaux portes enregistres\n");
			const int32 rc = shell->Run();

			// Parite avec NogeApp::OnClose : sauvegarde du projet si modifie.
			if (sProject.IsModified())
				sProject.Save();

			g_shell = nullptr;
			return static_cast<int>(rc);
		}

		void NogeeShellEnableOcclusionProbe(bool prefs) noexcept {
			g_probe.enabled = true;
			g_probe.testPrefs = prefs;
		}

		void NogeeShellReproduceConquerorLabCondition() noexcept {
			g_probe.noMaskBody = true;
		}

	} // namespace noge
} // namespace nkentseu
