// =============================================================================
// NkNN.h — briques de réseau de neurones (NKAI, Phase 2) — header PARAPLUIE.
//
// Regroupe les concepts du module, désormais séparés par fichier :
//   • NkDense.h        — couche entièrement connectée
//   • NkActivations.h  — Relu / Sigmoid / Tanh
//   • NkLosses.h       — MSELoss / CrossEntropyLoss (+ OneHot)
//   • NkTransformer.h  — LayerNorm affine / attention multi-têtes / bloc / GPT
//   • NkLlama.h        — RMSNorm / attention à positions par rotation / SwiGLU / bloc « moderne »
//   • NkRnn.h          — cellules récurrentes GRU / LSTM + déroulé séquence
//   • NkDropout.h      — couche de Dropout inversé (régularisation)
//   • NkSequential.h   — conteneur de modèle générique (chaîne des couches)
//   • NkArchitectures.h— architectures prêtes à l'emploi (NkMLP, NkCNN)
// Inclure ce header suffit pour tout le module. Namespace : nkentseu::ai::nn.
// =============================================================================
#pragma once

#include "NKNN/NkDense.h"
#include "NKNN/NkConv.h"
#include "NKNN/NkActivations.h"
#include "NKNN/NkLosses.h"
#include "NKNN/NkTransformer.h"
#include "NKNN/NkLlama.h"
#include "NKNN/NkRnn.h"
#include "NKNN/NkDropout.h"
#include "NKNN/NkSequential.h"
#include "NKNN/NkArchitectures.h"
