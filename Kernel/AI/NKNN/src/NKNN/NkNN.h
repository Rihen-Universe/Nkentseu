// =============================================================================
// NkNN.h — briques de réseau de neurones (NKAI, Phase 2) — header PARAPLUIE.
//
// Regroupe les concepts du module, désormais séparés par fichier :
//   • NkDense.h        — couche entièrement connectée
//   • NkActivations.h  — Relu / Sigmoid / Tanh
//   • NkLosses.h       — MSELoss / CrossEntropyLoss (+ OneHot)
// Inclure ce header suffit pour tout le module. Namespace : nkentseu::ai::nn.
// =============================================================================
#pragma once

#include "NKNN/NkDense.h"
#include "NKNN/NkConv.h"
#include "NKNN/NkActivations.h"
#include "NKNN/NkLosses.h"
