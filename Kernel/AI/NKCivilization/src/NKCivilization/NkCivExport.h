// =============================================================================
// NKCivilization/NkCivExport.h — export texte/CSV exploitable (Phase 5,
// Jalon 3 « visualiser & analyser »).
//
// Honnêteté de portée : PAS de rendu temps réel dans le moteur (non traité,
// voir ROADMAP.md « Jalon 3 » pour la limite assumée) — ici, un export
// PÉRIODIQUE et réutilisable hors moteur (tableur, script d'analyse) de :
//   - la « démographie »/ressources tick par tick (combien d'agents actifs/
//     terminés/arrivés/tombés, ressources restantes) — ExportTimelineCsv ;
//   - les statistiques agrégées par agent, telles que déjà calculées par
//     civ::NkCivAnalyzer (déjà livré, INCHANGÉ) — ExportAgentsCsv.
//
// Lecture PURE des données PUBLIQUES déjà capturées par NkCivRecorder /
// NkCivAnalyzer : aucune re-simulation, aucune modification de ces deux
// classes. Écriture via NKFileSystem/NkFile (même pattern que
// NkCivRecorder::SaveToFile). Namespace : nkentseu::ai::civ.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKCivilization/NkCivRecorder.h"
#include "NKCivilization/NkCivAnalyzer.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			class NkCivExporter {
				public:
					// Une ligne par TICK (en-tête inclus) :
					//   tick,active,done,reachedGoal,fell,resourcesRemaining
					// `active`/`done`/`reachedGoal`/`fell` = nombre d'agents dans cet
					// état CE tick (comptés depuis les bitmasks NK_CIV_REC_* déjà
					// enregistrés). false si le journal est vide ou l'écriture échoue.
					static bool ExportTimelineCsv(const NkCivRecorder &journal, const char *path);

					// Une ligne par AGENT (en-tête inclus), stats déjà calculées par
					// NkCivAnalyzer::Analyze (aucun recalcul ici) :
					//   turnOrder,startState,finalState,steps,blockedCount,movedCount,
					//   resourcesCollected,distanceTravelled,reachedGoal,fell,
					//   arrivalTick,maxConsecutiveBlocks
					// false si l'analyseur n'a aucun agent ou l'écriture échoue.
					static bool ExportAgentsCsv(const NkCivAnalyzer &analyzer, const char *path);
			};

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
