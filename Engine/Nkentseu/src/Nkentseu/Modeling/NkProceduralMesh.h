#pragma once
// =============================================================================
// Nkentseu/Modeling/NkProceduralMesh.h
// =============================================================================
// Génération procédurale de meshes de base.
// Produit des NkEditableMesh ou directement des renderer::NkMeshDesc.
// =============================================================================
#include "NkEditableMesh.h"

namespace nkentseu {
    namespace proc {

        struct NkPlaneParams  { float32 w=1, h=1; uint32 segW=1, segH=1; };
        struct NkCubeParams   { float32 size=1.f; uint32 segs=1; };
        struct NkSphereParams { float32 r=0.5f; uint32 rings=16, segs=32; };
        struct NkCylParams    { float32 r=0.5f, h=1.f; uint32 segs=32, rings=1; bool caps=true; };
        struct NkConeParams   { float32 r=0.5f, h=1.f; uint32 segs=32; };
        struct NkTorusParams  { float32 majorR=1.f, minorR=0.25f; uint32 majorSegs=48, minorSegs=12; };
        struct NkGridParams   { float32 w=10.f, h=10.f; uint32 segW=10, segH=10; };
        struct NkIcoParams    { float32 r=0.5f; uint32 subdivs=2; };

        [[nodiscard]] NkEditableMesh Plane   (const NkPlaneParams&  p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Cube    (const NkCubeParams&   p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Sphere  (const NkSphereParams& p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Cylinder(const NkCylParams&    p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Cone    (const NkConeParams&   p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Torus   (const NkTorusParams&  p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Grid    (const NkGridParams&   p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Icosphere(const NkIcoParams&   p = {}) noexcept;
        [[nodiscard]] NkEditableMesh Arrow   (float32 len=1.f, float32 headR=0.1f) noexcept;
        [[nodiscard]] NkEditableMesh Capsule (float32 r=0.5f, float32 h=1.f, uint32 segs=32) noexcept;

    } // namespace proc
} // namespace nkentseu
