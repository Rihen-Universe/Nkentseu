// =============================================================================
// NkGen.h — modèles génératifs (NKAI, Phase 6) — header PARAPLUIE.
//
// Regroupe les modèles génératifs du module :
//   • NkAutoencoder.h — auto-encodeur (compression + reconstruction/génération)
//   • NkDiffusion.h   — DDPM jouet 2D CPU (débruitage itératif, Ho et al. 2020)
//   • NkMesh.h        — voxels -> maillage de triangles + export OBJ
// Livré : auto-encodeur, VAE dense/conv 2D, VAE 3D voxels, diffusion 2D jouet,
// maillage (Surface Nets, OBJ).
// (à venir : GAN, marching cubes, conditionnement image/texte — cf ROADMAP.)
// Namespace : nkentseu::ai::gen.
// =============================================================================
#pragma once

#include "NKGen/NkAutoencoder.h"
#include "NKGen/NkVAE.h"
#include "NKGen/NkConvVAE.h"
#include "NKGen/NkVoxelVAE.h"
#include "NKGen/NkDiffusion.h"
#include "NKGen/NkMesh.h"
