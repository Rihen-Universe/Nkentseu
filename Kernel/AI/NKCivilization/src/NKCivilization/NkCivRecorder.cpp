// =============================================================================
// NKCivilization/NkCivRecorder.cpp — implémentation de l'enregistreur de
// trajectoires (Phase 5, outils d'observation) : journal en mémoire +
// sérialisation binaire versionnée `.nkciv` (magic "NCIV", little-endian,
// même pattern que le `.nkmec` de NkMeshEditRecorder).
// =============================================================================
#include "NKCivilization/NkCivRecorder.h"

#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			// ── Petit écriveur/lecteur d'octets (little-endian, pattern NkEditMesh) ──
			namespace {
				struct CvW {
						NkVector<uint8> &b;

						void U8(uint8 v) {
							b.PushBack(v);
						}

						void U32(uint32 v) {
							b.PushBack((uint8)(v & 0xFF));
							b.PushBack((uint8)((v >> 8) & 0xFF));
							b.PushBack((uint8)((v >> 16) & 0xFF));
							b.PushBack((uint8)((v >> 24) & 0xFF));
						}
				};

				struct CvR {
						const uint8 *d;
						uint32 n;
						uint32 p;
						bool ok;

						CvR(const uint8 *dd, uint32 nn) : d(dd), n(nn), p(0), ok(true) {
						}

						uint8 U8() {
							if (p + 1 > n) {
								ok = false;
								return 0;
							}
							return d[p++];
						}

						uint32 U32() {
							if (p + 4 > n) {
								ok = false;
								return 0;
							}
							uint32 v = (uint32)d[p] | ((uint32)d[p + 1] << 8) | ((uint32)d[p + 2] << 16) |
									   ((uint32)d[p + 3] << 24);
							p += 4;
							return v;
						}
				};

				static const uint32 NK_CIV_REC_MAGIC = 0x4E434956u; // "NCIV"
				static const uint32 NK_CIV_REC_VERSION = 1u;
			} // namespace

			void NkCivRecorder::Clear() {
				mGridSize = 0;
				mGoal = 0;
				mHoles.Clear();
				mResources.Clear();
				mStarts.Clear();
				mFrames.Clear();
				mInTick = false;
			}

			void NkCivRecorder::BeginSession(const NkCivGridState &grid, const NkVector<uint32> &starts) {
				Clear();
				mGridSize = grid.Size();
				mGoal = grid.GoalState();
				// Scan de la grille ENCORE INTACTE (avant tout tick) : trous et
				// cases ressource disponibles forment l'en-tête statique du monde.
				const uint32 total = mGridSize * mGridSize;
				for (uint32 s = 0; s < total; ++s) {
					if (grid.IsHole(s))
						mHoles.PushBack(s);
					if (grid.HasResource(s))
						mResources.PushBack(s);
				}
				for (nk_size i = 0; i < starts.Size(); ++i)
					mStarts.PushBack(starts[i]);
			}

			void NkCivRecorder::BeginTick(uint32 tick) {
				NkCivRecFrame frame;
				frame.tick = tick;
				mFrames.PushBack(frame);
				mInTick = true;
			}

			void NkCivRecorder::RecordAgent(uint32 turnOrder, uint32 state, uint8 action, uint8 flags) {
				if (!mInTick || mFrames.Empty())
					return;
				NkCivRecAgentSample sample;
				sample.turnOrder = turnOrder;
				sample.state = state;
				sample.action = action;
				sample.flags = flags;
				mFrames[mFrames.Size() - 1].agents.PushBack(sample);
			}

			void NkCivRecorder::EndTick(uint32 resourcesRemaining) {
				if (!mInTick || mFrames.Empty())
					return;
				mFrames[mFrames.Size() - 1].resourcesRemaining = resourcesRemaining;
				mInTick = false;
			}

			bool NkCivRecorder::Equals(const NkCivRecorder &other) const {
				if (mGridSize != other.mGridSize || mGoal != other.mGoal)
					return false;
				if (mHoles.Size() != other.mHoles.Size() || mResources.Size() != other.mResources.Size() ||
					mStarts.Size() != other.mStarts.Size() || mFrames.Size() != other.mFrames.Size())
					return false;
				for (nk_size i = 0; i < mHoles.Size(); ++i)
					if (mHoles[i] != other.mHoles[i])
						return false;
				for (nk_size i = 0; i < mResources.Size(); ++i)
					if (mResources[i] != other.mResources[i])
						return false;
				for (nk_size i = 0; i < mStarts.Size(); ++i)
					if (mStarts[i] != other.mStarts[i])
						return false;
				for (nk_size f = 0; f < mFrames.Size(); ++f) {
					const NkCivRecFrame &a = mFrames[f];
					const NkCivRecFrame &b = other.mFrames[f];
					if (a.tick != b.tick || a.resourcesRemaining != b.resourcesRemaining ||
						a.agents.Size() != b.agents.Size())
						return false;
					for (nk_size k = 0; k < a.agents.Size(); ++k) {
						const NkCivRecAgentSample &sa = a.agents[k];
						const NkCivRecAgentSample &sb = b.agents[k];
						if (sa.turnOrder != sb.turnOrder || sa.state != sb.state || sa.action != sb.action ||
							sa.flags != sb.flags)
							return false;
					}
				}
				return true;
			}

			void NkCivRecorder::Serialize(NkVector<uint8> &out) const {
				out.Clear();
				CvW w{out};
				w.U32(NK_CIV_REC_MAGIC);
				w.U32(NK_CIV_REC_VERSION);
				// En-tête statique du monde.
				w.U32(mGridSize);
				w.U32(mGoal);
				w.U32((uint32)mHoles.Size());
				for (nk_size i = 0; i < mHoles.Size(); ++i)
					w.U32(mHoles[i]);
				w.U32((uint32)mResources.Size());
				for (nk_size i = 0; i < mResources.Size(); ++i)
					w.U32(mResources[i]);
				w.U32((uint32)mStarts.Size());
				for (nk_size i = 0; i < mStarts.Size(); ++i)
					w.U32(mStarts[i]);
				// Frames (une par tick).
				w.U32((uint32)mFrames.Size());
				for (nk_size f = 0; f < mFrames.Size(); ++f) {
					const NkCivRecFrame &frame = mFrames[f];
					w.U32(frame.tick);
					w.U32(frame.resourcesRemaining);
					w.U32((uint32)frame.agents.Size());
					for (nk_size k = 0; k < frame.agents.Size(); ++k) {
						const NkCivRecAgentSample &s = frame.agents[k];
						w.U32(s.turnOrder);
						w.U32(s.state);
						w.U8(s.action);
						w.U8(s.flags);
					}
				}
			}

			bool NkCivRecorder::Deserialize(const uint8 *data, uint32 size) {
				Clear();
				CvR r(data, size);
				if (r.U32() != NK_CIV_REC_MAGIC)
					return false;
				if (r.U32() != NK_CIV_REC_VERSION)
					return false; // version inconnue : refuser plutôt que mal lire
				mGridSize = r.U32();
				mGoal = r.U32();
				const uint32 nHoles = r.U32();
				for (uint32 i = 0; i < nHoles && r.ok; ++i)
					mHoles.PushBack(r.U32());
				const uint32 nRes = r.U32();
				for (uint32 i = 0; i < nRes && r.ok; ++i)
					mResources.PushBack(r.U32());
				const uint32 nStarts = r.U32();
				for (uint32 i = 0; i < nStarts && r.ok; ++i)
					mStarts.PushBack(r.U32());
				const uint32 nFrames = r.U32();
				for (uint32 f = 0; f < nFrames && r.ok; ++f) {
					NkCivRecFrame frame;
					frame.tick = r.U32();
					frame.resourcesRemaining = r.U32();
					const uint32 nAgents = r.U32();
					for (uint32 k = 0; k < nAgents && r.ok; ++k) {
						NkCivRecAgentSample s;
						s.turnOrder = r.U32();
						s.state = r.U32();
						s.action = r.U8();
						s.flags = r.U8();
						if (r.ok)
							frame.agents.PushBack(s);
					}
					if (r.ok)
						mFrames.PushBack(frame);
				}
				if (!r.ok)
					Clear();
				return r.ok;
			}

			bool NkCivRecorder::SaveToFile(const char *path) const {
				NkVector<uint8> bytes;
				Serialize(bytes);
				return NkFile::WriteAllBytes(path, bytes);
			}

			bool NkCivRecorder::LoadFromFile(const char *path) {
				NkVector<uint8> bytes = NkFile::ReadAllBytes(path);
				if (bytes.Empty())
					return false;
				return Deserialize(bytes.Data(), (uint32)bytes.Size());
			}

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
