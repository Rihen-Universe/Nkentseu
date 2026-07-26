
#pragma once
// =============================================================================
// Nkentseu/Crowd/NkCrowdSim.h — Simulation de foules (agents)
// Voir NkLocomotion.h pour NkCrowdAgent et NkCrowdSystem.
// Ce fichier contient les utilitaires de navigation spatiale.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKCollision/NkColTypes.h"
#include "Noge/Anim/NkLocomotion.h"

namespace nkentseu {
	using namespace math;

	// Spatial grid for crowd queries (agents cherchent leurs voisins)
	class NkCrowdGrid {
		public:
			void Init(float32 cellSize, collision::NkAABB3D worldBounds) noexcept;
			void Clear() noexcept;
			void Insert(NkEntityId entity, NkVec3f pos) noexcept;
			void QueryNeighbors(NkVec3f pos, float32 radius, NkVector<NkEntityId> &out) const noexcept;

		private:
			float32 mCellSize = 2.f;
			collision::NkAABB3D mBounds;
			NkVector<NkVector<NkPair<NkEntityId, NkVec3f>>> mCells;
			uint32 mCellsX = 0, mCellsZ = 0;
			uint32 CellIdx(NkVec3f pos) const noexcept;
	};

	// Manager global de foule
	class NkCrowdManager {
		public:
			NkCrowdGrid grid;
			float32 neighborRadius = 2.f;
			uint32 maxAgents = 10000;

			void Update(ecs::NkWorld &world, float32 dt) noexcept;
			void RebuildGrid(ecs::NkWorld &world) noexcept;
	};
} // namespace nkentseu
