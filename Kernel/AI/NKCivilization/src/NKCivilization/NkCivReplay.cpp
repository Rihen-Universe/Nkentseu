// =============================================================================
// NKCivilization/NkCivReplay.cpp — implémentation du rejoueur de trajectoires
// (Phase 5, outils d'observation) : lecture PURE d'un journal NkCivRecorder,
// sans re-simulation.
// =============================================================================
#include "NKCivilization/NkCivReplay.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			bool NkCivReplayer::Open(const NkCivRecorder *journal) {
				mJournal = journal;
				mCursor = -1;
				return mJournal != nullptr && mJournal->FrameCount() > 0;
			}

			bool NkCivReplayer::NextTick() {
				if (!mJournal)
					return false;
				if (mCursor + 1 >= (int32)mJournal->FrameCount())
					return false;
				++mCursor;
				return true;
			}

			uint32 NkCivReplayer::AgentState(uint32 turnOrder) const {
				if (!mJournal)
					return 0;
				if (mCursor < 0) {
					// Avant le premier tick : position = départ enregistré.
					if (turnOrder < mJournal->Starts().Size())
						return mJournal->Starts()[turnOrder];
					return 0;
				}
				const NkCivRecFrame &frame = mJournal->Frame((uint32)mCursor);
				for (nk_size i = 0; i < frame.agents.Size(); ++i)
					if (frame.agents[i].turnOrder == turnOrder)
						return frame.agents[i].state;
				// Agent absent de cette frame (ne devrait pas arriver, un
				// échantillon est écrit pour chaque agent à chaque tick) :
				// dernière position connue en remontant le journal.
				for (int32 f = mCursor - 1; f >= 0; --f) {
					const NkCivRecFrame &fr = mJournal->Frame((uint32)f);
					for (nk_size i = 0; i < fr.agents.Size(); ++i)
						if (fr.agents[i].turnOrder == turnOrder)
							return fr.agents[i].state;
				}
				if (turnOrder < mJournal->Starts().Size())
					return mJournal->Starts()[turnOrder];
				return 0;
			}

			bool NkCivReplayer::AgentDone(uint32 turnOrder) const {
				if (!mJournal || mCursor < 0)
					return false;
				const NkCivRecFrame &frame = mJournal->Frame((uint32)mCursor);
				for (nk_size i = 0; i < frame.agents.Size(); ++i)
					if (frame.agents[i].turnOrder == turnOrder)
						return (frame.agents[i].flags & NK_CIV_REC_DONE) != 0;
				return false;
			}

			bool NkCivReplayer::VerifyExact(const NkCivRecorder &a, const NkCivRecorder &b, uint32 *outFirstDiff) {
				if (a.GridSize() != b.GridSize() || a.Goal() != b.Goal() || a.AgentCount() != b.AgentCount() ||
					a.Holes().Size() != b.Holes().Size() || a.ResourceCells().Size() != b.ResourceCells().Size() ||
					a.FrameCount() != b.FrameCount()) {
					if (outFirstDiff)
						*outFirstDiff = 0xFFFFFFFFu;
					return false;
				}
				for (nk_size i = 0; i < a.Holes().Size(); ++i)
					if (a.Holes()[i] != b.Holes()[i]) {
						if (outFirstDiff)
							*outFirstDiff = 0xFFFFFFFFu;
						return false;
					}
				for (nk_size i = 0; i < a.ResourceCells().Size(); ++i)
					if (a.ResourceCells()[i] != b.ResourceCells()[i]) {
						if (outFirstDiff)
							*outFirstDiff = 0xFFFFFFFFu;
						return false;
					}
				for (nk_size i = 0; i < a.AgentCount(); ++i)
					if (a.Starts()[i] != b.Starts()[i]) {
						if (outFirstDiff)
							*outFirstDiff = 0xFFFFFFFFu;
						return false;
					}
				for (uint32 f = 0; f < a.FrameCount(); ++f) {
					const NkCivRecFrame &fa = a.Frame(f);
					const NkCivRecFrame &fb = b.Frame(f);
					bool same = (fa.tick == fb.tick) && (fa.resourcesRemaining == fb.resourcesRemaining) &&
								(fa.agents.Size() == fb.agents.Size());
					if (same) {
						for (nk_size k = 0; k < fa.agents.Size() && same; ++k) {
							const NkCivRecAgentSample &sa = fa.agents[k];
							const NkCivRecAgentSample &sb = fb.agents[k];
							if (sa.turnOrder != sb.turnOrder || sa.state != sb.state || sa.action != sb.action ||
								sa.flags != sb.flags)
								same = false;
						}
					}
					if (!same) {
						if (outFirstDiff)
							*outFirstDiff = f;
						return false;
					}
				}
				return true;
			}

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
