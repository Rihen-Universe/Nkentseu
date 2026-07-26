// =============================================================================
// NKCivilization/NkCivExport.cpp — implémentation de l'export CSV (Phase 5,
// Jalon 3).
// =============================================================================
#include "NKCivilization/NkCivExport.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			bool NkCivExporter::ExportTimelineCsv(const NkCivRecorder &journal, const char *path) {
				if (journal.FrameCount() == 0)
					return false;

				NkString csv("tick,active,done,reachedGoal,fell,resourcesRemaining\n");
				for (uint32 f = 0; f < journal.FrameCount(); ++f) {
					const NkCivRecFrame &frame = journal.Frame(f);
					uint32 active = 0;
					uint32 done = 0;
					uint32 reachedGoal = 0;
					uint32 fell = 0;
					for (nk_size k = 0; k < frame.agents.Size(); ++k) {
						const NkCivRecAgentSample &s = frame.agents[k];
						if (s.flags & NK_CIV_REC_ACTIVE)
							++active;
						if (s.flags & NK_CIV_REC_DONE)
							++done;
						if (s.flags & NK_CIV_REC_REACHED_GOAL)
							++reachedGoal;
						if (s.flags & NK_CIV_REC_FELL)
							++fell;
					}
					csv += NkString::Format("%u,%u,%u,%u,%u,%u\n", frame.tick, active, done, reachedGoal, fell,
											 frame.resourcesRemaining);
				}

				return NkFile::WriteAllText(path, csv);
			}

			bool NkCivExporter::ExportAgentsCsv(const NkCivAnalyzer &analyzer, const char *path) {
				if (analyzer.AgentCount() == 0)
					return false;

				NkString csv("turnOrder,startState,finalState,steps,blockedCount,movedCount,resourcesCollected,"
							  "distanceTravelled,reachedGoal,fell,arrivalTick,maxConsecutiveBlocks\n");
				for (uint32 a = 0; a < analyzer.AgentCount(); ++a) {
					const NkCivAgentStats &st = analyzer.AgentStat(a);
					csv += NkString::Format("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u\n", st.turnOrder, st.startState,
											 st.finalState, st.steps, st.blockedCount, st.movedCount,
											 st.resourcesCollected, st.distanceTravelled, st.reachedGoal ? 1u : 0u,
											 st.fell ? 1u : 0u, st.arrivalTick, st.maxConsecutiveBlocks);
				}

				return NkFile::WriteAllText(path, csv);
			}

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
