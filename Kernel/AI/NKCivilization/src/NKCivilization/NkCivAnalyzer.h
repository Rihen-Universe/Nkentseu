// =============================================================================
// NKCivilization/NkCivAnalyzer.h — l'analyseur de trajectoires (Phase 5,
// outils d'observation) : extrait des statistiques d'un journal NkCivRecorder
// SANS jamais re-simuler — pure lecture/agrégation :
//   - PAR AGENT : pas effectués, blocages subis, ressources collectées, tick
//     d'arrivée au but (-1 si jamais arrivé), distance parcourue.
//   - GLOBALES : densité d'occupation des cases (heatmap TEXTE), taux de
//     contention des ressources (ressources consommées / total), distance
//     totale parcourue par la société.
//   - ÉVÉNEMENTS REMARQUABLES : détection simple loggée lisiblement (blocage
//     mutuel répété = un agent bloqué N fois de suite consécutives ; agent
//     qui domine les ressources = un agent collecte une part largement
//     majoritaire du total consommé).
// Namespace : nkentseu::ai::civ.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCivilization/NkCivRecorder.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			// Statistiques d'UN agent, extraites du journal complet.
			struct NkCivAgentStats {
					uint32 turnOrder = 0;
					uint32 startState = 0;
					uint32 finalState = 0;
					uint32 steps = 0;			  // ticks où l'agent a effectivement joué (actif)
					uint32 blockedCount = 0;	  // fois bloqué (collision)
					uint32 movedCount = 0;		  // fois où la position a réellement changé
					uint32 resourcesCollected = 0;
					uint32 distanceTravelled = 0; // somme des distances Manhattan par mouvement réel
					bool reachedGoal = false;
					bool fell = false;
					int32 arrivalTick = -1; // tick (index dans le journal) d'arrivée au but/trou, -1 sinon
					uint32 maxConsecutiveBlocks = 0; // plus longue série de blocages consécutifs
			};

			// Un événement remarquable détecté (texte prêt à logger).
			struct NkCivEvent {
					NkString description;
			};

			// Statistiques GLOBALES + heatmap + événements, extraites d'un journal.
			class NkCivAnalyzer {
				public:
					// Analyse intégrale du journal (une passe). false si le journal
					// est vide ou incohérent (en-tête sans agents).
					bool Analyze(const NkCivRecorder &journal);

					uint32 AgentCount() const {
						return (uint32)mAgentStats.Size();
					}

					const NkCivAgentStats &AgentStat(uint32 turnOrder) const {
						return mAgentStats[turnOrder];
					}

					uint32 TotalTicks() const {
						return mTotalTicks;
					}

					uint32 TotalDistanceTravelled() const {
						return mTotalDistance;
					}

					uint32 TotalResourcesConsumed() const {
						return mTotalResourcesConsumed;
					}

					uint32 TotalResourcesAvailable() const {
						return mTotalResourcesAvailable;
					}

					// Taux de contention = fraction des ressources disponibles qui a
					// effectivement été consommée (0..1).
					float32 ResourceContentionRate() const {
						return (mTotalResourcesAvailable == 0)
								   ? 0.0f
								   : (float32)mTotalResourcesConsumed / (float32)mTotalResourcesAvailable;
					}

					// Heatmap TEXTE (une ligne par rangée de la grille) : chaque case
					// affiche le nombre de fois où un agent quelconque s'y est trouvé
					// (occupation cumulée sur toute la session), en base36 (0-9A-Z),
					// '#' si >= 36. Case but = 'G', trou = 'O' (si jamais occupée par
					// aucun agent, sinon le compte prime).
					const NkVector<NkString> &HeatmapRows() const {
						return mHeatmapRows;
					}

					const NkVector<NkCivEvent> &Events() const {
						return mEvents;
					}

				private:
					NkVector<NkCivAgentStats> mAgentStats;
					NkVector<uint32> mOccupancy; // taille gridSize*gridSize : nb de visites cumulées
					NkVector<NkString> mHeatmapRows;
					NkVector<NkCivEvent> mEvents;
					uint32 mTotalTicks = 0;
					uint32 mTotalDistance = 0;
					uint32 mTotalResourcesConsumed = 0;
					uint32 mTotalResourcesAvailable = 0;
			};

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
