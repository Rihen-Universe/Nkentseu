// =============================================================================
// NKCivilization/NkCivGridState.h — le substrat partagé du micro-monde
// (Phase 5, Jalon 1) : espace (grille N×N, trous, but commun) + RESSOURCES
// consommables (compétition : une case ressource est consommée par le PREMIER
// agent qui y entre — première interaction émergente mesurable).
//
// Règles de déplacement identiques à rl::NkGridWorld::Step (0=haut 1=bas
// 2=gauche 3=droite ; sortir de la grille = rester sur place), mais PURES :
// la grille partagée ne porte aucun curseur d'agent — les positions vivent
// dans les composants NkCivPosition des entités NKECS.
//
// Jalon 2 — RÈGLE SOCIALE (2026-07-25) : généralisation des ressources d'un
// booléen "consommée/pas consommée" à un STOCK borné (capacité) qui peut se
// RENOUVELER chaque tick (`regenPerTick`), et une bascule
// `EnableSocialSharing` qui, quand active, exempte les cases ressource de la
// règle de collision (cf. NkCivAgentSystem::Execute) : plusieurs agents
// peuvent alors CO-OCCUPER une case ressource le même tick et y récolter
// chacun leur tour (au lieu que le premier arrivé bloque physiquement les
// suivants). Par défaut (capacity=1, regenPerTick=0, social OFF) le
// comportement est BIT-À-BIT IDENTIQUE à l'ancien schéma binaire premier-
// arrivé — condition A (baseline) de l'expérience Jalon 2. Condition B
// (capacity>1, regenPerTick>0, social ON) permet la récolte partagée
// (coopération structurelle) au lieu de l'accaparement exclusif. Voir
// ROADMAP.md « Jalon 2 — société émergente » pour le protocole de test.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			class NkCivGridState {
				public:
					NkCivGridState(uint32 size, uint32 goalState, const NkVector<uint32> &holes);

					uint32 Size() const {
						return mSize;
					}

					uint32 GoalState() const {
						return mGoal;
					}

					bool IsHole(uint32 state) const;

					bool IsGoal(uint32 state) const {
						return state == mGoal;
					}

					// Règle de déplacement pure (aucun état modifié) : état visé depuis
					// `state` par `action` — même géométrie que rl::NkGridWorld.
					uint32 NextState(uint32 state, uint32 action) const;

					// ── Ressources (stock borné, compétition ou récolte partagée) ────
					// `capacity` = stock initial ET maximum de la case ; `regenPerTick`
					// = unités régénérées par RegenerateTick() (0 = ressource classique
					// à usage unique, comportement Jalon 1 inchangé quand capacity=1).
					void AddResource(uint32 state, uint32 capacity = 1, uint32 regenPerTick = 0);
					bool HasResource(uint32 state) const;	  // stock encore > 0 sur CETTE case ?
					bool IsResourceCell(uint32 state) const; // case enregistrée comme ressource (stock ou pas)
					uint32 Stock(uint32 state) const;		  // stock courant de CETTE case (0 si pas une ressource)
					// Consomme 1 unité du stock de `state` si encore disponible.
					// Retourne true si CET appel a prélevé une unité, false sinon.
					// Capacity=1/regen=0 : redevient l'ancienne sémantique « premier
					// arrivé consomme tout, plus rien ensuite ».
					bool ConsumeResource(uint32 state);

					// Régénère toutes les cases ressource de `regenPerTick` (borné par
					// leur capacité) — à appeler UNE fois par tick. No-op si toutes les
					// ressources ont regenPerTick=0 (condition A, comportement figé).
					void RegenerateTick();

					nk_size ResourcesTotal() const {
						return mResources.Size();
					}

					nk_size ResourcesRemaining() const;

					// Met à jour capacité/régénération d'une case ressource DÉJÀ
					// enregistrée (ne touche pas au stock courant, sauf clamp si le
					// nouveau plafond est plus bas que le stock actuel). No-op si
					// `state` n'est pas une case ressource existante (utiliser
					// AddResource() pour en créer une nouvelle). Additif, n'affecte
					// aucun appelant existant (personne ne l'appelait avant Jalon 4).
					// Usage : scénarios « what-if » (Jalon 4) qui changent une règle
					// de ressource À UN TICK DONNÉ d'une simulation DÉJÀ EN COURS,
					// sur un objet grille VIVANT (pas de reconstruction depuis un
					// journal — NkCivRecorder ne capture pas le stock par case).
					void SetResourceParams(uint32 state, uint32 capacity, uint32 regenPerTick);

					// ── Jalon 2 — règle sociale ────────────────────────────────────────
					// Quand ON : les cases ressource sont exemptées de la règle de
					// collision (NkCivAgentSystem::Execute) -> co-occupation/récolte
					// partagée possible. OFF par défaut (= condition A, baseline).
					void EnableSocialSharing(bool on) {
						mSocialSharing = on;
					}

					bool SocialSharingEnabled() const {
						return mSocialSharing;
					}

				private:
					uint32 mSize;
					uint32 mGoal;
					NkVector<uint32> mHoles;
					NkVector<uint32> mResources;	 // cases ressource
					NkVector<uint32> mCapacity;	 // parallèle à mResources : stock maximum
					NkVector<uint32> mStock;		 // parallèle à mResources : stock courant
					NkVector<uint32> mRegenPerTick; // parallèle à mResources : régénération/tick
					bool mSocialSharing = false;	 // règle sociale Jalon 2 (défaut = OFF, condition A)
			};

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
