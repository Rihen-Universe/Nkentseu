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
#include "Nogee/Panels/ViewportPanel.h"
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
#include <cstring>

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

			// ═════════════════════════════════════════════════════════════════
			// SONDE GLISSER-DEPOSER (--dragdrop-test) — meme regle que la sonde
			// d'occultation : une mesure reproductible ne peut pas cliquer a la
			// main. La sonde pilote ctx.input depuis l'overlay (posee en fin de
			// frame N, lue par BeginFrame N+1), avec les rects ECRAN releves par
			// les panneaux eux-memes — jamais une geometrie devinee.
			//
			// Scenarios, dans l'ordre (les negatifs d'abord, l'arbre est encore
			// simple ; le positif §7 change la hierarchie) :
			//   0. depliage de la racine (les TreeNode NKGui naissent replies) ;
			//   1. §7 NEGATIF : glisser Enfant_A, lacher sur le Viewport (cible
			//      typee "asset") -> AUCUN changement de parent ;
			//   2. §7 NEGATIF : glisser la Racine sur son propre descendant
			//      Enfant_B -> REFUS cycle du panneau, parent inchange ;
			//   3. §7 POSITIF : glisser Enfant_A sur Enfant_B -> SetParent ;
			//   4. §9 POSITIF : glisser la 1re carte du Content Browser sur le
			//      Viewport -> livraison journalisee (spawn non cable) ;
			//   5. §9 NEGATIF : re-glisser la carte, lacher sur une ligne de
			//      l'Outliner (cible typee "entity") -> pas de 2e livraison.
			// ═════════════════════════════════════════════════════════════════
			struct DragProbe {
					bool enabled = false;
					int32 frame = 0;	///< frames depuis le demarrage
					int32 scenario = 0; ///< 0..5, puis 6 = verdict
					// ⚠️ CADENCE EN SECONDES, PAS EN FRAMES (paye : a ~140 fps, des
					// phases de 2 frames font ~40 ms — chaque paire d'appuis tombait
					// sous le seuil de DOUBLE-CLIC de NKGui (0,40 s, NkGuiInput.h) :
					// rename inline au lieu d'un glisser, et zero drag demarre).
					float32 t = 0.f;	   ///< temps ecoule DANS le geste courant
					float32 waitT = 0.f;   ///< temps passe a attendre une geometrie
					bool sawDrag = false;   ///< controle positif : le glisser a demarre
					bool diagDone = false;  ///< diagnostic mi-geste emis
					bool diag2Done = false; ///< diagnostic en plein APPUI emis
					int32 attempts = 0;		///< tentatives du geste courant (cf. RETRY)
					int32 checks = 0;
					int32 fails = 0;
					bool focusDone = false; ///< FocusPanel du scenario courant fait
					bool reported = false;
					char expectPath[256] = {}; ///< chemin attendu au scenario 4
					ecs::NkEntityId idRacine{}, idA{}, idB{};
			};

			DragProbe g_drag;
			WorldOutlinerPanel *g_dragOutliner = nullptr;
			ContentBrowserPanel *g_dragContent = nullptr;
			ViewportPanel *g_dragViewport = nullptr;
			ecs::NkWorld *g_dragWorld = nullptr;

			void DragCheck(bool ok, const char *what) {
				++g_drag.checks;
				if (!ok)
					++g_drag.fails;
				char msg[256];
				std::snprintf(msg, sizeof(msg), "[SONDE-DD] TEMOIN %s : %s\n", ok ? "OK   " : "ECHEC", what);
				logger.Info(msg);
			}

			ecs::NkEntityId DragParentOf(ecs::NkEntityId id) {
				const ecs::NkParent *p = g_dragWorld ? g_dragWorld->Get<ecs::NkParent>(id) : nullptr;
				return p ? p->entity : ecs::NkEntityId::Invalid();
			}

			bool DragChildrenContains(ecs::NkEntityId parent, ecs::NkEntityId child) {
				const ecs::NkChildren *ch = g_dragWorld ? g_dragWorld->Get<ecs::NkChildren>(parent) : nullptr;
				if (!ch)
					return false;
				for (nk_uint32 i = 0; i < ch->count; ++i)
					if (ch->children[i] == child)
						return true;
				return false;
			}

			void DragVerdict(NkEditorFrameContext &) {
				char msg[192];
				std::snprintf(msg, sizeof(msg), "[SONDE-DD] VERDICT : %d/%d temoins OK%s\n",
							  g_drag.checks - g_drag.fails, g_drag.checks,
							  g_drag.fails == 0 ? " — SONDE OK" : " — SONDE ECHEC");
				logger.Info(msg);
				g_drag.reported = true;
				if (g_shell)
					g_shell->RequestClose();
			}

			void DragOverlay(NkEditorFrameContext &ec, void *) {
				if (!g_drag.enabled || g_drag.reported)
					return;
				nkgui::NkGuiContext &ui = ec.Ui();
				++g_drag.frame;

				// Garde-fou global : si la mesure ne converge pas, on conclut en
				// echec explicite plutot que de tourner sans fin.
				if (g_drag.frame > 20000) { // ~60 s meme a 300 fps : garde-fou, pas une cadence
					DragCheck(false, "delai global depasse (20000 frames) — la sonde ne converge pas");
					DragVerdict(ec);
					return;
				}
				if (g_drag.frame < 20)
					return; // laisser le dock se mettre en place

				auto center = [](const nkgui::NkRect &r) {
					return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
				};
				auto rowCenter = [&](ecs::NkEntityId id, nkgui::NkVec2 *out) -> bool {
					const WorldOutlinerPanel::RowProbe *rp =
						g_dragOutliner ? g_dragOutliner->ProbeRow(id) : nullptr;
					if (!rp)
						return false;
					*out = center(rp->rect);
					return true;
				};
				auto pose = [&](nkgui::NkVec2 p, bool down) {
					ui.input.mousePos = p;
					ui.input.mouseDown[0] = down;
				};

				const float32 dt = (ui.input.dt > 0.f && ui.input.dt < 0.1f) ? ui.input.dt : (1.f / 60.f);

				// ── Scenario 0 : depliage de la racine ────────────────────────
				if (g_drag.scenario == 0) {
					if (!g_drag.focusDone && g_shell) {
						g_shell->FocusPanel("World Outliner");
						g_drag.focusDone = true;
						return;
					}
					nkgui::NkVec2 pA;
					if (rowCenter(g_drag.idA, &pA)) { // deja depliee
						g_drag.scenario = 1;
						g_drag.t = 0.f;
						g_drag.waitT = 0.f;
						g_drag.sawDrag = false;
						g_drag.diagDone = false;
						g_drag.diag2Done = false;
						g_drag.focusDone = true; // meme panneau au scenario 1
						logger.Info("[SONDE-DD] scenario 0 : racine depliee, lignes TEMOIN visibles\n");
						return;
					}
					nkgui::NkVec2 pR;
					if (!rowCenter(g_drag.idRacine, &pR)) {
						g_drag.waitT += dt;
						if (g_drag.waitT > 4.f) {
							DragCheck(false, "scenario 0 : ligne TEMOIN_Racine jamais mesuree");
							DragVerdict(ec);
						}
						return;
					}
					// clic LENT : un cycle par seconde — deux appuis a moins de
					// 0,40 s seraient un DOUBLE-CLIC (rename), pas deux clics.
					g_drag.t += dt;
					if (g_drag.t < 0.20f)
						pose(pR, false);
					else if (g_drag.t < 0.45f)
						pose(pR, true);
					else if (g_drag.t < 1.00f)
						pose(pR, false);
					else {
						g_drag.t = 0.f; // re-mesurer : si toujours repliee, re-cliquer
						g_drag.diagDone = false;
						g_drag.diag2Done = false;
					}
					return;
				}

				// ── Verdict ───────────────────────────────────────────────────
				if (g_drag.scenario >= 6) {
					DragVerdict(ec);
					return;
				}

				// ── S3 : le SUCCES du geste cache sa propre geometrie (paye a la
				// premiere execution : A reparentee sous B, noeud B REPLIE, ligne A
				// plus rendue -> « geometrie jamais mesuree » alors que le journal
				// prouvait « reparentage applique »). Si l'etat du MONDE dit que le
				// geste a abouti, on joue les temoins au lieu d'exiger une
				// geometrie que le succes a fait disparaitre.
				if (g_drag.scenario == 3 && DragParentOf(g_drag.idA) == g_drag.idB) {
					DragCheck(g_drag.sawDrag, "controle positif du scenario : le glisser a demarre");
					DragCheck(true, "S3 positif : Enfant_A a pour parent Enfant_B");
					DragCheck(DragChildrenContains(g_drag.idB, g_drag.idA),
							  "S3 positif : les enfants d'Enfant_B contiennent Enfant_A");
					DragCheck(!DragChildrenContains(g_drag.idRacine, g_drag.idA),
							  "S3 positif : Enfant_A a quitte les enfants de la racine");
					g_drag.scenario = 4;
					g_drag.t = 0.f;
					g_drag.waitT = 0.f;
					g_drag.sawDrag = false;
					g_drag.diagDone = false;
						g_drag.diag2Done = false;
					g_drag.focusDone = false;
					ui.input.mouseDown[0] = false; // souris relachee, etat propre
					return;
				}

				// ── Scenarios 1..5 : un GESTE de glisser generique ───────────
				// Sources/cibles mesurees CHAQUE frame (le dock peut bouger).
				nkgui::NkVec2 src{}, dst{};
				bool haveGeom = false;
				switch (g_drag.scenario) {
					case 1: { // §7 negatif : A -> viewport (type refuse)
						nkgui::NkRect vz;
						haveGeom = rowCenter(g_drag.idA, &src) &&
								   (g_dragViewport && g_dragViewport->ProbeRect(&vz));
						if (haveGeom)
							dst = center(vz);
						break;
					}
					case 2: // §7 negatif : Racine -> Enfant_B (cycle)
						if (!g_drag.focusDone && g_shell) {
							g_shell->FocusPanel("World Outliner");
							g_drag.focusDone = true;
							return;
						}
						haveGeom = rowCenter(g_drag.idRacine, &src) && rowCenter(g_drag.idB, &dst);
						break;
					case 3: // §7 positif : A -> B
						if (!g_drag.focusDone && g_shell) {
							g_shell->FocusPanel("World Outliner");
							g_drag.focusDone = true;
							return;
						}
						haveGeom = rowCenter(g_drag.idA, &src) && rowCenter(g_drag.idB, &dst);
						break;
					case 4: { // §9 positif : carte -> viewport
						if (!g_drag.focusDone && g_shell) {
							g_shell->FocusPanel("Content Browser");
							g_drag.focusDone = true;
							return;
						}
						nkgui::NkRect card, vz;
						const char *rel = nullptr;
						haveGeom = g_dragContent && g_dragContent->ProbeCard(&card, &rel) &&
								   g_dragViewport && g_dragViewport->ProbeRect(&vz);
						if (haveGeom) {
							src = center(card);
							dst = center(vz);
							std::snprintf(g_drag.expectPath, sizeof(g_drag.expectPath), "%s", rel);
						}
						break;
					}
					case 5: { // §9 negatif : carte -> ligne Outliner (type refuse)
						nkgui::NkRect card;
						haveGeom = g_dragContent && g_dragContent->ProbeCard(&card, nullptr) &&
								   rowCenter(g_drag.idRacine, &dst);
						if (haveGeom)
							src = center(card);
						break;
					}
				}

				if (!haveGeom) {
					// §9 : aucune carte FICHIER visible — les dossiers occupent la
					// vue initiale, les fichiers sont sous le pli (paye : clipOk=0,
					// puis carte=0 une fois la visibilite exigee). Un utilisateur
					// ferait defiler : la sonde pose la molette au centre du
					// panneau jusqu'a ce qu'une carte fichier entre dans le clip
					// (le defilement se cale en butee basse, ou vivent les
					// fichiers — l'ordre du navigateur met les dossiers d'abord).
					if ((g_drag.scenario == 4 || g_drag.scenario == 5) && g_dragContent) {
						nkgui::NkRect area;
						nkgui::NkRect cardTmp;
						if (!g_dragContent->ProbeCard(&cardTmp, nullptr) && g_dragContent->ProbeArea(&area)) {
							pose(center(area), false);
							// DIFFEREE : la molette brute est consommee par EndFrame
							// avant d'etre lue (meme famille que l'anti-gel).
							ui.input.AddWheelDeferred(-1.f); // negatif = vers le bas
						}
					}
					g_drag.waitT += dt;
					if (g_drag.waitT > 4.f) {
						// Diagnostic : QUELLE geometrie manque — on ne conclut
						// jamais d'un echec muet.
						nkgui::NkVec2 tmp;
						nkgui::NkRect tr;
						const char *rel = nullptr;
						char msg[256];
						std::snprintf(msg, sizeof(msg),
									  "scenario %d : geometrie jamais mesuree (racine=%d A=%d B=%d "
									  "carte=%d viewport=%d)",
									  g_drag.scenario, rowCenter(g_drag.idRacine, &tmp) ? 1 : 0,
									  rowCenter(g_drag.idA, &tmp) ? 1 : 0, rowCenter(g_drag.idB, &tmp) ? 1 : 0,
									  (g_dragContent && g_dragContent->ProbeCard(&tr, &rel)) ? 1 : 0,
									  (g_dragViewport && g_dragViewport->ProbeRect(&tr)) ? 1 : 0);
						DragCheck(false, msg);
						g_drag.scenario++;
						g_drag.t = 0.f;
						g_drag.waitT = 0.f;
						g_drag.sawDrag = false;
						g_drag.diagDone = false;
						g_drag.diag2Done = false;
						g_drag.focusDone = false;
					}
					return;
				}
				g_drag.waitT = 0.f;

				// Controle positif du scenario : un glisser a-t-il DEMARRE ?
				if (ui.dragActive)
					g_drag.sawDrag = true;

				g_drag.t += dt;
				const float32 t = g_drag.t;
				if (!g_drag.diagDone && t >= 0.85f) { // diagnostic a mi-geste
					g_drag.diagDone = true;
					char msg[256];
					std::snprintf(msg, sizeof(msg),
								  "[SONDE-DD] scenario %d mi-geste : dragActive=%d type='%s' charge=%d octets "
								  "| src=(%d,%d) activeId=%u hotIdPrev=%u candidateId=%u\n",
								  g_drag.scenario, ui.dragActive ? 1 : 0, ui.dragType,
								  (int)ui.dragPayloadSize, (int)src.x, (int)src.y, (unsigned)ui.activeId,
								  (unsigned)ui.hotIdPrev, (unsigned)ui.dragCandidateId);
					logger.Info(msg);
				}
				if (!g_drag.diag2Done && t >= 0.40f) { // diagnostic en plein APPUI
					g_drag.diag2Done = true;
					char msg[256];
					std::snprintf(msg, sizeof(msg),
								  "[SONDE-DD] scenario %d plein-appui : src=(%d,%d) activeId=%u "
								  "hotIdPrev=%u candidateId=%u down=%d fraicheurCarte=%.2fs "
								  "portesPied{hoveredWin=%u curWin=%u clipOk=%d}\n",
								  g_drag.scenario, (int)src.x, (int)src.y, (unsigned)ui.activeId,
								  (unsigned)ui.hotIdPrev, (unsigned)ui.dragCandidateId,
								  ui.input.mouseDown[0] ? 1 : 0,
								  g_dragContent ? (ui.time - g_dragContent->ProbeCardTime()) : -1.f,
								  g_dragContent ? (unsigned)g_dragContent->ProbeGateHoveredWin() : 0u,
								  g_dragContent ? (unsigned)g_dragContent->ProbeGateCurWin() : 0u,
								  g_dragContent ? (g_dragContent->ProbeGateClipContains() ? 1 : 0) : -1);
					logger.Info(msg);
				}
				if (t < 0.20f)
					pose(src, false); // survol source (hotIdPrev)
				else if (t < 0.50f)
					pose(src, true); // appui : armement
				else if (t < 0.70f)
					pose({src.x + 14.f, src.y}, true); // > seuil : le glisser part
				else if (t < 1.00f)
					pose(dst, true); // survol de la cible
				else if (t < 1.30f)
					pose(dst, false); // relachement
				else if (t >= 1.60f) { // reglement, puis verification
					// RETRY : le front d'appui pose depuis l'overlay rate parfois
					// (intermittent, surtout caches froids apres un build — la
					// signature mesuree : hotIdPrev pose, down=1, activeId=0). Un
					// utilisateur re-cliquerait ; la sonde refait donc le geste,
					// EN LE DISANT — un rate absorbe en silence serait une mesure
					// qui ment. Les temoins ne sont evalues qu'apres un geste dont
					// le glisser a demarre, ou l'epuisement des tentatives.
					if (!g_drag.sawDrag && g_drag.attempts < 3) {
						++g_drag.attempts;
						char rmsg[160];
						std::snprintf(rmsg, sizeof(rmsg),
									  "[SONDE-DD] scenario %d RETRY %d/3 : le glisser n'a pas demarre, "
									  "geste rejoue\n",
									  g_drag.scenario, g_drag.attempts);
						logger.Info(rmsg);
						g_drag.t = 0.f;
						g_drag.diagDone = false;
						g_drag.diag2Done = false;
						return;
					}
					DragCheck(g_drag.sawDrag, "controle positif du scenario : le glisser a demarre");
					switch (g_drag.scenario) {
						case 1:
							DragCheck(DragParentOf(g_drag.idA) == g_drag.idRacine,
									  "S1 hors cible : parent d'Enfant_A INCHANGE (racine)");
							DragCheck(g_dragViewport && g_dragViewport->DropCount() == 0,
									  "S1 hors cible : le viewport (type 'asset') n'a RIEN recu");
							break;
						case 2:
							DragCheck(!DragParentOf(g_drag.idRacine).IsValid(),
									  "S2 cycle : la racine n'a PAS ete reparentee sous son descendant");
							break;
						case 3:
							DragCheck(DragParentOf(g_drag.idA) == g_drag.idB,
									  "S3 positif : Enfant_A a pour parent Enfant_B");
							DragCheck(DragChildrenContains(g_drag.idB, g_drag.idA),
									  "S3 positif : les enfants d'Enfant_B contiennent Enfant_A");
							DragCheck(!DragChildrenContains(g_drag.idRacine, g_drag.idA),
									  "S3 positif : Enfant_A a quitte les enfants de la racine");
							break;
						case 4:
							DragCheck(g_dragViewport && g_dragViewport->DropCount() == 1,
									  "S4 §9 : le viewport a recu UNE livraison");
							DragCheck(g_dragViewport &&
										  std::strcmp(g_dragViewport->LastDroppedPath(), g_drag.expectPath) == 0,
									  "S4 §9 : chemin livre INTACT (== chemin de la carte)");
							break;
						case 5:
							DragCheck(g_dragViewport && g_dragViewport->DropCount() == 1,
									  "S5 §9 hors cible : AUCUNE 2e livraison (type 'entity' refuse)");
							break;
					}
					g_drag.scenario++;
					g_drag.t = 0.f;
					g_drag.sawDrag = false;
					g_drag.diagDone = false;
						g_drag.diag2Done = false;
					g_drag.focusDone = false;
					g_drag.attempts = 0;
				}
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
				if (g_drag.enabled) { // la sonde drag-drop vise ces entites-la
					g_drag.idRacine = racine;
					g_drag.idA = enfantA;
					g_drag.idB = enfantB;
				}
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
			// Zone CENTRE : la cible du glisser d'assets (§9). Pas un viewport —
			// le rendu de scene n'existe pas sur ce chemin, et le panneau le dit
			// (cf. ViewportPanel.h).
			static ViewportPanel sViewport;

			sOutliner.Bind(&sWorld, &sScene, &sSel, &sHist);
			sDetails.Bind(&sWorld, &sSel, &sHist);
			sContent.Init(&sAssets, projectDir);

			shell->AddPanel(&sViewport);
			shell->AddPanel(&sOutliner);
			shell->AddPanel(&sDetails);
			shell->AddPanel(&sContent);

			// Cablage de la sonde drag-drop (--dragdrop-test) : les panneaux
			// mesurent leur geometrie reelle, la sonde la consomme.
			if (g_drag.enabled) {
				sOutliner.EnableProbe(true);
				sContent.EnableProbe(true);
				sViewport.EnableProbe(true);
				g_dragOutliner = &sOutliner;
				g_dragContent = &sContent;
				g_dragViewport = &sViewport;
				g_dragWorld = &sWorld;
			}

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
			// UNE sonde par execution (le shell n'a qu'un overlay, et melanger
			// deux protocoles melangerait leurs mesures) : drag-drop si demandee,
			// sinon occultation.
			if (g_drag.enabled) {
				shell->SetOverlay(&DragOverlay, nullptr);
				if (g_probe.enabled)
					logger.Info("[SONDE] --occlusion-test IGNORE : --dragdrop-test est deja actif "
								"(une sonde par execution)\n");
				logger.Info("[SONDE-DD] activee : glisser-deposer §7/§9 pilote par frames\n");
			} else {
				shell->SetOverlay(&ProbeOverlay, nullptr);
			}

			if (g_probe.enabled && !g_drag.enabled)
				logger.Info("[SONDE] activee : mesure du routeur d'occultation a l'execution\n");

			logger.Info("[Nogee/Shell] coquille prete — 4 panneaux portes enregistres\n");
			const int32 rc = shell->Run();

			// Parite avec NogeApp::OnClose : sauvegarde du projet si modifie.
			if (sProject.IsModified())
				sProject.Save();

			// ── Arret de l'AssetManager PENDANT que le device RHI vit ────────
			// Mesure (2026-08-17, gdb) : sans cet appel, le premier Shutdown est
			// celui du DESTRUCTEUR statique, a l'atexit — apres la destruction du
			// device. `mDevice` pendouille, les entrees sont encore chargees,
			// DestroyTexture ecrit dans un device mort : SIGSEGV systematique a
			// toute sortie propre (RequestClose). Personne ne l'avait vu parce
			// que Nogee n'etait jusqu'ici tue que par timeout, jamais ferme.
			sAssets.Shutdown();

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

		void NogeeShellEnableDragDropProbe() noexcept {
			g_drag.enabled = true;
		}

	} // namespace noge
} // namespace nkentseu
