// =============================================================================
// NKAgent/NkGoalStack.h — pile de buts actifs (le futur) (NKAI, Phase 4, Jalon 3).
//
// La PLANIFICATION la plus simple qui décompose une tâche en sous-buts : une
// pile LIFO de NkAgentGoal. Décomposer « atteindre la ressource PUIS le but
// final » = empiler le but final D'ABORD puis le sous-but (la ressource)
// PAR-DESSUS — le sommet de la pile (Current()) est TOUJOURS le sous-but à
// poursuivre maintenant ; une fois atteint (ou abandonné, cf. NkAgentGoal::
// maxSteps), il est dépilé et le but sous-jacent redevient automatiquement le
// but actif. C'est le mécanisme de bascule « à chaque tick l'agent poursuit
// le but actif courant » exigé par le Jalon 3, sans planificateur de recherche
// complexe — une pile suffit à exprimer un plan séquentiel de sous-buts.
// Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKAgent/NkAgentGoal.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			class NkGoalStack {
				public:
					// Empile un nouveau but : il devient immédiatement le but ACTIF
					// (sommet de pile), au-dessus de tout but déjà présent.
					void Push(const NkAgentGoal &goal) {
						mGoals.PushBack(goal);
					}

					// Dépile le but courant (achevé ou abandonné) : le but précédent
					// (sous-jacent) redevient actif. No-op si la pile est vide.
					void Pop() {
						if (!mGoals.IsEmpty())
							mGoals.PopBack();
					}

					bool IsEmpty() const {
						return mGoals.IsEmpty();
					}

					nk_size Size() const {
						return mGoals.Size();
					}

					// Le but ACTIF courant = sommet de la pile. Ne pas appeler si
					// IsEmpty() (comme Back() sur un conteneur vide).
					NkAgentGoal &Current() {
						return mGoals[mGoals.Size() - 1];
					}

					const NkAgentGoal &Current() const {
						return mGoals[mGoals.Size() - 1];
					}

					// Accès en lecture à la pile complète, du but le plus profond (index 0,
					// le plan « final ») au but actif (index Size()-1) — utile pour
					// afficher/journaliser le plan courant.
					const NkAgentGoal &At(nk_size indexFromBottom) const {
						return mGoals[indexFromBottom];
					}

					void Clear() {
						mGoals.Clear();
					}

				private:
					NkVector<NkAgentGoal> mGoals;
			};

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
