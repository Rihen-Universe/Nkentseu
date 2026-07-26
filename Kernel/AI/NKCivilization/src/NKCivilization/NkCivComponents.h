// =============================================================================
// NKCivilization/NkCivComponents.h — composants NKECS du micro-monde (Phase 5).
//
// Le vrai substrat ECS du Jalon 1 : chaque agent de la civilisation est une
// ENTITÉ NKECS portant ces deux composants, et c'est NkCivAgentSystem
// (un ecs::NkSystem standard) qui les fait agir chaque tick — plus de boucle
// `for` à la main comme dans le prototype pré-ECS de NKCivilizationTest.
//
// Choix de couche : NKCivilization est dans Kernel/AI — dépendre du pont Noge
// (Engine/Noge/ECS/Components/AI/NkAgentComponent) inverserait les couches
// (Kernel -> Engine). On rebâtit donc l'équivalent minimal directement sur
// NKECS pur (Kernel/Runtime/NKECS), avec la même philosophie que le pont
// Noge : le composant ne POSSÈDE PAS l'agent (agent::NkAgent embarque table Q
// + mémoire + perception, non trivialement copiable — or NkWorld::Add<T>
// copie T par valeur) ; il le RÉFÉRENCE (pointeur non possédant), la durée de
// vie étant gérée par l'appelant (la démo, le jeu, un futur asset store).
// Namespace : nkentseu::ai::civ.
// =============================================================================
#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKAgent/NkAgent.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			// Position d'un habitant sur la grille partagée (état brut y*N+x, même
			// convention que rl::NkGridWorld). Composant POD pur -> pool d'archétype.
			struct NkCivPosition {
					uint32 state = 0;
			};
			NK_COMPONENT(NkCivPosition)

			// L'habitant : référence sa politique apprise (via agent::NkAgent, non
			// possédé) + son état de simulation et ses compteurs d'interaction.
			struct NkCivAgentRef {
					agent::NkAgent *agent = nullptr; // non possédé : politique apprise (Phase 4)

					uint32 turnOrder = 0; // ordre d'action déterministe dans le tick

					// ── État de simulation (piloté par NkCivAgentSystem) ─────────────
					bool done = false;		  // a fini (but atteint ou tombé dans un trou)
					bool reachedGoal = false; // fini SUR le but
					uint32 steps = 0;		  // pas effectués

					// ── Mesures d'interaction (le jalon « ça interagit ») ────────────
					uint32 blockedCount = 0;		  // fois où la case visée était occupée (collision)
					uint32 resourcesCollected = 0; // ressources consommées (compétition, 1er arrivé)
			};
			NK_COMPONENT(NkCivAgentRef)

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
