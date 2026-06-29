#pragma once
// =============================================================================
// NKCollision.h — Include unique du module de collision 2D + 3D (ZÉRO STL).
//
//   #include "NKCollision/NKCollision.h"
//   nkentseu::collision::NkWorld world;
//   uint32 a = world.AddBody(nkentseu::collision::NkShape::Sphere({0,0,0}, 1.f));
//   uint32 b = world.AddBody(nkentseu::collision::NkShape::Sphere({1.5f,0,0}, 1.f));
//   world.Step();
//   for (auto& p : world.Pairs()) { /* p.manifold.normal / depth */ }
// =============================================================================
#include "NKCollision/NkColTypes.h"
#include "NKCollision/NkColShapes.h"
#include "NKCollision/NkColTests.h"
#include "NKCollision/NkCollisionWorld.h"
