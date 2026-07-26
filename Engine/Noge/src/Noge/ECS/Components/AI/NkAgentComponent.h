#pragma once
// -----------------------------------------------------------------------------
// FICHIER: Noge/ECS/Components/AI/NkAgentComponent.h
// DESCRIPTION: Pont ECS -> NKAgent (Kernel/AI/NKAgent) — IA de gameplay / RL.
//
// Même philosophie que NkCollider3D (Noge/ECS/Components/Physics/NkPhysics.h) :
// le COMPOSANT ne POSSÈDE PAS la ressource lourde. Un agent::NkAgent embarque
// une table Q (rl::NkQLearning), une mémoire d'événements bornée
// (agent::NkAgentMemory) et un encodeur de perception — tout cela n'est ni
// trivialement copiable ni destiné à vivre dans un pool d'archétype ECS
// (NkWorld::Add<T> copie/affecte T par valeur). Le composant est donc une
// simple RÉFÉRENCE (pointeur non possédant) vers une instance agent::NkAgent
// et son environnement rl::NkGridWorld, tous deux possédés ailleurs (ex. par
// la démo/le jeu, ou par un futur NkAgentAssetStore) — exactement comme
// NkCollider3D::physicsBodyId référence un corps possédé par
// physics::NkPhysicsWorld (NkPhysicsSystem).
//
// NkAgentSystem (Noge/ECS/Systems/NkAgentSystem.h) consomme ce composant et
// appelle agent::NkAgent::Step() une fois par tick pour chaque entité valide.
// -----------------------------------------------------------------------------

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKAgent/NkAgent.h"
#include "NKRL/NkGridWorld.h"

namespace nkentseu {
	namespace ecs {

		// =============================================================================
		// NkAgentComponent — référence un agent::NkAgent + son environnement RL.
		// =============================================================================
		// Ni `agent` ni `environment` ne sont possédés par le composant : leur durée de
		// vie doit dépasser celle de l'entité (ou être gérée explicitement par
		// l'appelant). NkAgentSystem se contente de piloter la boucle
		// perception -> décision -> action -> mémorisation à travers ces pointeurs.
		struct NkAgentComponent {
				ai::agent::NkAgent *agent = nullptr;		// non possédé : mémoire+perception+politique
				ai::rl::NkGridWorld *environment = nullptr; // non possédé : monde RL (NkEnvironment)

				// ── Configuration ─────────────────────────────────────────────────────────
				bool learn = true;				  // true = ε-greedy + mise à jour TD ; false = évaluation gloutonne pure
				bool autoResetOnDone = true;	  // relance un nouvel épisode automatiquement (Reset()) si terminé
				uint32 maxStepsPerEpisode = 100; // borne de sécurité (0 = illimité, coupé seulement par `done`)

				// ── État runtime (piloté par NkAgentSystem, lecture possible côté jeu) ──────
				uint32 currentState = 0;	// état brut courant de l'environnement
				bool hasState = false;		// currentState est-il valide (Reset() déjà appelé) ?
				uint32 stepsInEpisode = 0; // pas effectués depuis le dernier Reset()

				uint32 lastAction = 0;
				float32 lastReward = 0.f;
				bool lastDone = false;

				// ── Statistiques cumulées (utile pour valider le jalon : taux de réussite) ──
				uint32 totalSteps = 0;
				uint32 episodesCompleted = 0;
				uint32 episodesReachedGoal = 0; // épisode terminé avec reward > 0.5 (but atteint, cf. NkGridWorld)
		};
		NK_COMPONENT(NkAgentComponent)

	} // namespace ecs
} // namespace nkentseu
