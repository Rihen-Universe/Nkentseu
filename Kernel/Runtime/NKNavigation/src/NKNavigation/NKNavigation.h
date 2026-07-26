#pragma once
// =============================================================================
// NKNavigation.h — Include unique du module de navigation (NavMesh + A*,
// ZÉRO STL).
//
//   #include "NKNavigation/NKNavigation.h"
//   nkentseu::nav::NkNavMesh mesh;
//   nkentseu::nav::NkNavMeshBuildDesc desc;
//   desc.areaMin = {-10.f, -1.f, -10.f};
//   desc.areaMax = {10.f, 1.f, 10.f};
//   desc.groundVerts = verts; desc.groundVertCount = vertCount;
//   desc.groundIndices = indices; desc.groundIndexCount = indexCount;
//   mesh.Build(desc);
//   nkentseu::NkVector<nkentseu::nav::NkVec3f> path;
//   mesh.FindPath(start, goal, path);
// =============================================================================
#include "NKNavigation/NkNavTypes.h"
#include "NKNavigation/NkNavMesh.h"
