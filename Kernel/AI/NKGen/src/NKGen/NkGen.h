// =============================================================================
// NkGen.h — modèles génératifs (NKAI, Phase 6) — header PARAPLUIE.
//
// Regroupe les modèles génératifs du module :
//   • NkAutoencoder.h — auto-encodeur (compression + reconstruction/génération)
//   • NkMesh.h        — voxels -> maillage de triangles + export OBJ
// (à venir : VAE, diffusion 2D, marching cubes, animation — cf ROADMAP.)
// Namespace : nkentseu::ai::gen.
// =============================================================================
#pragma once

#include "NKGen/NkAutoencoder.h"
#include "NKGen/NkVAE.h"
#include "NKGen/NkConvVAE.h"
#include "NKGen/NkVoxelVAE.h"
#include "NKGen/NkMesh.h"
