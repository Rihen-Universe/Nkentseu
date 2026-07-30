// =============================================================================
// NKCivilization/NkCivRecorder.h — l'enregistreur de trajectoires du micro-monde
// (Phase 5, outils d'observation) : à chaque tick de NkCivAgentSystem, capture
// l'état pertinent (position de CHAQUE agent, action choisie, ressources
// restantes, événements — collision/blocage, collecte, arrivée au but, chute)
// dans un journal EN MÉMOIRE, puis sérialisation binaire versionnée sur disque
// (format `.nkciv`, magic "NCIV" + version — même pattern que le `.nkmec` de
// NkMeshEditRecorder, cf. NKRenderer/Mesh/NkEditMesh.h).
//
// Le journal est AUTONOME : il embarque l'en-tête statique du monde (taille de
// grille, but, trous, cases ressource, départs des agents) + une frame par
// tick — un `.nkciv` seul suffit donc à rejouer (NkCivReplayer) et analyser
// (NkCivAnalyzer) une simulation SANS la re-simuler.
// Namespace : nkentseu::ai::civ.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCivilization/NkCivGridState.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			// ── Événements d'un agent dans un tick (bitmask, uint8) ─────────────
			enum : uint8 {
				NK_CIV_REC_ACTIVE = 1 << 0,		  // l'agent a joué ce tick (non terminé)
				NK_CIV_REC_BLOCKED = 1 << 1,	  // collision : case visée occupée -> resté sur place
				NK_CIV_REC_MOVED = 1 << 2,		  // la position a effectivement changé
				NK_CIV_REC_COLLECTED = 1 << 3,	  // ressource consommée CE tick (premier arrivé)
				NK_CIV_REC_REACHED_GOAL = 1 << 4, // arrivé au but commun CE tick
				NK_CIV_REC_FELL = 1 << 5,		  // tombé dans un trou CE tick
				NK_CIV_REC_DONE = 1 << 6		  // terminé (cumulatif : but ou trou, ticks suivants inclus)
			};

			// Pas d'action enregistrable (agent terminé/inactif ce tick).
			const uint8 NK_CIV_REC_NO_ACTION = 0xFF;

			// Échantillon d'UN agent dans UN tick. `state` = position APRÈS l'action
			// du tick (== position d'avant si bloqué ou terminé).
			struct NkCivRecAgentSample {
					uint32 turnOrder = 0;
					uint32 state = 0;
					uint8 action = NK_CIV_REC_NO_ACTION; // 0..3, NK_CIV_REC_NO_ACTION sinon
					uint8 flags = 0;					 // bitmask NK_CIV_REC_*
			};

			// Une frame = l'état pertinent du monde à la fin d'UN tick.
			struct NkCivRecFrame {
					uint32 tick = 0;
					uint32 resourcesRemaining = 0;		  // ressources encore disponibles APRÈS le tick
					NkVector<NkCivRecAgentSample> agents; // un échantillon par agent, ordre = turnOrder
			};

			// Journal de trajectoires : enregistre une session, la persiste (.nkciv).
			class NkCivRecorder {
				public:
					void Clear();

					// Ouvre une session AVANT le premier tick : capture l'en-tête
					// statique (taille, but, trous, cases ressource — scannées sur la
					// grille encore intacte) + départs des agents (indexés turnOrder).
					void BeginSession(const NkCivGridState &grid, const NkVector<uint32> &starts);

					// ── Capture d'un tick (appelée par NkCivAgentSystem) ─────────────
					void BeginTick(uint32 tick);
					void RecordAgent(uint32 turnOrder, uint32 state, uint8 action, uint8 flags);
					void EndTick(uint32 resourcesRemaining);

					// ── Lecture (en-tête) ────────────────────────────────────────────
					uint32 GridSize() const {
						return mGridSize;
					}

					uint32 Goal() const {
						return mGoal;
					}

					const NkVector<uint32> &Holes() const {
						return mHoles;
					}

					const NkVector<uint32> &ResourceCells() const {
						return mResources;
					}

					const NkVector<uint32> &Starts() const {
						return mStarts;
					}

					uint32 AgentCount() const {
						return (uint32)mStarts.Size();
					}

					// ── Lecture (frames) ─────────────────────────────────────────────
					uint32 FrameCount() const {
						return (uint32)mFrames.Size();
					}

					const NkCivRecFrame &Frame(uint32 i) const {
						return mFrames[i];
					}

					// Égalité STRUCTURELLE complète (en-tête + toutes les frames) —
					// la preuve « le rejeu reproduit exactement les états enregistrés ».
					bool Equals(const NkCivRecorder &other) const;

					// ── Sérialisation binaire versionnée (magic "NCIV") ──────────────
					void Serialize(NkVector<uint8> &out) const;
					bool Deserialize(const uint8 *data, uint32 size);

					// Persistance disque au format `.nkciv` (via NKFileSystem/NkFile).
					bool SaveToFile(const char *path) const;
					bool LoadFromFile(const char *path);

				private:
					uint32 mGridSize = 0;
					uint32 mGoal = 0;
					NkVector<uint32> mHoles;	 // cases trou
					NkVector<uint32> mResources; // cases ressource (état initial, avant consommation)
					NkVector<uint32> mStarts;	 // départ par agent (index = turnOrder)
					NkVector<NkCivRecFrame> mFrames;
					bool mInTick = false;
			};

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
