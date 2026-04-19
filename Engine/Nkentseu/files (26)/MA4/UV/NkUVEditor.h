
#pragma once
// =============================================================================
// Nkentseu/Modeling/NkUVEditor.h — Editeur UV interactif
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Nkentseu/Modeling/NkEditableMesh.h"
#include "Nkentseu/Modeling/NkUndoStack.h"

namespace nkentseu {
using namespace math;

struct NkUVIsland {
    NkVector<uint32> faceIndices;
    NkAABB2f         bounds;
    float32          rotation = 0.f;
    NkVec2f          pivot    = {};
    bool             selected = false;
};

class NkUVEditor {
public:
    NkUVEditor() noexcept = default;

    void SetMesh(NkEditableMesh* mesh) noexcept;
    void BuildIslands() noexcept;

    // ── Navigation ────────────────────────────────────────────────────────
    NkVec2f  viewOffset = {};
    float32  zoom       = 1.f;
    void Pan(NkVec2f delta) noexcept { viewOffset = viewOffset + delta; }
    void Zoom(float32 delta) noexcept { zoom = NkClamp(zoom + delta, 0.1f, 50.f); }

    // ── Sélection ────────────────────────────────────────────────────────
    void SelectIsland(uint32 idx, bool add = false) noexcept;
    void SelectAll()   noexcept;
    void DeselectAll() noexcept;
    void SelectIslandsInRect(NkAABB2f uvRect) noexcept;

    // ── Transforms UV ────────────────────────────────────────────────────
    void TranslateSelected(NkVec2f delta) noexcept;
    void RotateSelected   (float32 angleDeg) noexcept;
    void ScaleSelected    (NkVec2f scale)   noexcept;
    void FlipHSelected    () noexcept;
    void FlipVSelected    () noexcept;

    // ── Unwrap ────────────────────────────────────────────────────────────
    void SmartUnwrap   (float32 angleLimit = 66.f) noexcept;
    void UnwrapFromSeams() noexcept;
    void PackIslands   (float32 margin = 0.02f) noexcept;
    void RelaxIslands  (uint32 iterations = 10) noexcept;  // LSCM/ABF

    // ── Rendu de l'éditeur UV ─────────────────────────────────────────────
    void Draw(renderer::NkRender2D& r2d, const NkRectF& viewport,
               nk_uint64 texHandle = 0) const noexcept;

    NkVector<NkUVIsland> islands;
    NkUndoStack*         undoStack = nullptr;

private:
    NkEditableMesh* mMesh = nullptr;
};
}
