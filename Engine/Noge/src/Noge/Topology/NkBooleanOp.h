
#pragma once
// =============================================================================
// Nkentseu/Modeling/NkBooleanOp.h — Opérations booléennes mesh (BSP-based)
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "Nkentseu/Modeling/NkHalfEdge.h"

namespace nkentseu {

enum class NkBoolType : uint8 { Union, Subtract, Intersect, Difference };

class NkBooleanOp {
public:
    // Opérations
    [[nodiscard]] static NkHalfEdgeMesh
    Union    (const NkHalfEdgeMesh& a, const NkHalfEdgeMesh& b) noexcept;
    [[nodiscard]] static NkHalfEdgeMesh
    Subtract (const NkHalfEdgeMesh& a, const NkHalfEdgeMesh& b) noexcept;
    [[nodiscard]] static NkHalfEdgeMesh
    Intersect(const NkHalfEdgeMesh& a, const NkHalfEdgeMesh& b) noexcept;
    [[nodiscard]] static NkHalfEdgeMesh
    Compute  (const NkHalfEdgeMesh& a, const NkHalfEdgeMesh& b,
               NkBoolType op) noexcept;

private:
    // BSP (Binary Space Partitioning) + Sutherland-Hodgman clipping
    struct BSPNode;
    static BSPNode* BuildBSP(const NkHalfEdgeMesh& mesh) noexcept;
    static void ClassifyMesh(const NkHalfEdgeMesh& mesh,
                               const BSPNode* bsp,
                               NkHalfEdgeMesh& inside,
                               NkHalfEdgeMesh& outside) noexcept;
};
}
