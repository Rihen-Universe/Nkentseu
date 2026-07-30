// =============================================================================
// NKCivilization/NkCivAnalyzer.cpp — implémentation de l'analyseur de
// trajectoires (Phase 5, outils d'observation) : une passe sur un journal
// NkCivRecorder déjà chargé, PAS de re-simulation.
// =============================================================================
#include "NKCivilization/NkCivAnalyzer.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			namespace {

				// Distance de Manhattan entre deux états y*N+x sur une grille `size`.
				uint32 ManhattanDist(uint32 a, uint32 b, uint32 size) {
					if (size == 0)
						return 0;
					const int32 ax = (int32)(a % size), ay = (int32)(a / size);
					const int32 bx = (int32)(b % size), by = (int32)(b / size);
					const int32 dx = ax > bx ? ax - bx : bx - ax;
					const int32 dy = ay > by ? ay - by : by - ay;
					return (uint32)(dx + dy);
				}

				// Compte -> caractère heatmap : '.'=0, '0'-'9'=1..10 (compte-1), puis
				// 'A'-'Z' pour 11..36, '#' au-delà (base36 décalée pour distinguer 0
				// visuellement d'une vraie visite unique).
				char HeatmapChar(uint32 count) {
					if (count == 0)
						return '.';
					const uint32 v = count - 1; // 0 -> '0' (une visite)
					if (v < 10)
						return (char)('0' + v);
					if (v < 36)
						return (char)('A' + (v - 10));
					return '#';
				}

			} // namespace

			bool NkCivAnalyzer::Analyze(const NkCivRecorder &journal) {
				mAgentStats.Clear();
				mOccupancy.Clear();
				mHeatmapRows.Clear();
				mEvents.Clear();
				mTotalTicks = 0;
				mTotalDistance = 0;
				mTotalResourcesConsumed = 0;
				mTotalResourcesAvailable = 0;

				const uint32 nAgents = journal.AgentCount();
				const uint32 gridSize = journal.GridSize();
				if (nAgents == 0 || gridSize == 0)
					return false;

				mTotalResourcesAvailable = (uint32)journal.ResourceCells().Size();
				mTotalTicks = journal.FrameCount();

				// ── Init par-agent (état de départ) ──────────────────────────────
				mAgentStats.Reserve(nAgents);
				for (uint32 a = 0; a < nAgents; ++a) {
					NkCivAgentStats st;
					st.turnOrder = a;
					st.startState = journal.Starts()[a];
					st.finalState = st.startState;
					mAgentStats.PushBack(st);
				}

				// ── Occupation cumulée : la case de DÉPART compte comme visite 1. ──
				const uint32 total = gridSize * gridSize;
				mOccupancy.Resize(total, 0u);
				for (uint32 a = 0; a < nAgents; ++a)
					if (journal.Starts()[a] < total)
						++mOccupancy[journal.Starts()[a]];

				// Série de blocages consécutifs en cours, par agent (turnOrder).
				NkVector<uint32> runBlocked;
				runBlocked.Resize(nAgents, 0u);

				// Meilleur candidat "blocage mutuel" : agent + longueur de la série.
				for (uint32 f = 0; f < journal.FrameCount(); ++f) {
					const NkCivRecFrame &frame = journal.Frame(f);
					for (nk_size k = 0; k < frame.agents.Size(); ++k) {
						const NkCivRecAgentSample &s = frame.agents[k];
						if (s.turnOrder >= nAgents)
							continue;
						NkCivAgentStats &st = mAgentStats[s.turnOrder];

						const bool active = (s.flags & NK_CIV_REC_ACTIVE) != 0;
						const bool blocked = (s.flags & NK_CIV_REC_BLOCKED) != 0;
						const bool moved = (s.flags & NK_CIV_REC_MOVED) != 0;
						const bool collected = (s.flags & NK_CIV_REC_COLLECTED) != 0;
						const bool reachedGoal = (s.flags & NK_CIV_REC_REACHED_GOAL) != 0;
						const bool fell = (s.flags & NK_CIV_REC_FELL) != 0;

						if (active)
							++st.steps;
						if (blocked) {
							++st.blockedCount;
							++runBlocked[s.turnOrder];
							if (runBlocked[s.turnOrder] > st.maxConsecutiveBlocks)
								st.maxConsecutiveBlocks = runBlocked[s.turnOrder];
						} else {
							runBlocked[s.turnOrder] = 0;
						}
						if (moved) {
							++st.movedCount;
							const uint32 d = ManhattanDist(st.finalState, s.state, gridSize);
							st.distanceTravelled += d;
							mTotalDistance += d;
							if (s.state < total)
								++mOccupancy[s.state];
						}
						if (collected) {
							++st.resourcesCollected;
							++mTotalResourcesConsumed;
						}
						if (reachedGoal) {
							st.reachedGoal = true;
							st.arrivalTick = (int32)f;
						}
						if (fell) {
							st.fell = true;
							st.arrivalTick = (int32)f;
						}
						st.finalState = s.state;
					}
				}

				// ── Heatmap texte (une ligne par rangée, la ligne haut d'abord) ────
				mHeatmapRows.Reserve(gridSize);
				for (uint32 y = 0; y < gridSize; ++y) {
					NkString row;
					for (uint32 x = 0; x < gridSize; ++x) {
						const uint32 s = y * gridSize + x;
						const uint32 cnt = mOccupancy[s];
						row.Append(' ');
						if (s == journal.Goal() && cnt == 0)
							row.Append('G');
						else if (cnt == 0) {
							bool isHole = false;
							for (nk_size i = 0; i < journal.Holes().Size(); ++i)
								if (journal.Holes()[i] == s) {
									isHole = true;
									break;
								}
							row.Append(isHole ? 'O' : '.');
						} else {
							row.Append(HeatmapChar(cnt));
						}
					}
					mHeatmapRows.PushBack(row);
				}

				// ── Détection d'événements remarquables ──────────────────────────
				// 1) Blocage mutuel répété : un agent bloqué >= 2 fois de suite.
				for (uint32 a = 0; a < nAgents; ++a) {
					const NkCivAgentStats &st = mAgentStats[a];
					if (st.maxConsecutiveBlocks >= 2) {
						NkCivEvent ev;
						ev.description = NkString("agent ");
						ev.description.Append((char)('0' + (a % 10)));
						ev.description.Append(" : blocage repete (");
						ev.description.Append((char)('0' + (st.maxConsecutiveBlocks % 10)));
						ev.description.Append(" ticks consecutifs bloque -- collision recurrente au meme goulot)");
						mEvents.PushBack(ev);
					}
				}
				// 2) Agent qui domine les ressources : sa part > 50% du total consommé
				//    (avec au moins 2 agents et au moins 1 ressource consommée).
				if (nAgents >= 2 && mTotalResourcesConsumed > 0) {
					for (uint32 a = 0; a < nAgents; ++a) {
						const NkCivAgentStats &st = mAgentStats[a];
						if (st.resourcesCollected * 2 > mTotalResourcesConsumed) {
							NkCivEvent ev;
							ev.description = NkString("agent ");
							ev.description.Append((char)('0' + (a % 10)));
							ev.description.Append(" domine la competition de ressources (");
							ev.description.Append((char)('0' + (st.resourcesCollected % 10)));
							ev.description.Append("/");
							ev.description.Append((char)('0' + (mTotalResourcesConsumed % 10)));
							ev.description.Append(" consommees)");
							mEvents.PushBack(ev);
						}
					}
				}

				return true;
			}

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
