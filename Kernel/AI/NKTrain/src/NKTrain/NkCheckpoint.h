// =============================================================================
// NKTrain/NkCheckpoint.h — checkpoint GÉNÉRIQUE modèle + optimiseur + boucle
// (NKAI, Jalon 2 : « durabilité »).
// -----------------------------------------------------------------------------
// Généralise pour N'IMPORTE QUEL modèle NKNN + optimiseur NKOptim le mécanisme
// prouvé dans NkGptTrainer/NkGptCore (checkpoint « NKGP » v4, poids + état Adam
// pour une reprise PARFAITE du schedule) : ici on ne dépend plus de rien de
// spécifique GPT (pas de BPE, pas de dims de modèle figées, pas de langues) —
// seulement `NkVector<NkVar>` (poids, N'IMPORTE QUEL modèle construit à partir de
// NKNN) et `optim::NkAdam` (l'optimiseur le plus utilisé dans NKAI).
//
// Format « NKTC » (NKTrain Checkpoint) v1 :
//   magic "NKTC" | version u32
//   paramCount u32 | pour chaque paramètre : rank u32, dims[rank] i64, data[numel] f32
//   hasOpt u8 | si 1 : stepCount i64, puis paramCount moments m PUIS paramCount moments v
//   hasState u8 | si 1 : epoch i64, globalStep i64, bestMetric f64, badEpochs i32
//
// `NkTrainState` capture ce qu'il faut pour REPRENDRE une boucle interrompue
// sans perdre le compteur de patience (early stopping) ni le pas global (LR
// schedule) : cf NkCallback.h (NkEarlyStopping::RestoreState) et NkTrain.h (Fit).
// Namespace : nkentseu::ai::train. Zéro STL (FILE* C, comme NKInfer/NKGpt).
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKOptim/NkOptim.h"

namespace nkentseu {
	namespace ai {
		namespace train {

			// État de boucle nécessaire à une reprise fidèle après interruption.
			struct NkTrainState {
					int64 epoch = 0;			  // dernière époque TERMINÉE
					int64 globalStep = 0;		  // pas de gradient global (scheduler LR)
					double bestMetric = 1.0e300; // meilleure métrique vue (early stopping)
					int32 badEpochs = 0;		  // époques consécutives sans amélioration
			};

			// Sauvegarde les poids `params`. Si `opt` est fourni (non nul), écrit AUSSI son
			// état (moments Adam 1er/2e + compteur de pas) => reprise SANS re-warmup ni pic
			// de perte. Si `state` est fourni, écrit aussi l'état de boucle générique
			// (époque/pas global/meilleure métrique/patience). true si succès.
			bool SaveCheckpoint(const char *path, const NkVector<NkVar> &params, const optim::NkAdam *opt = nullptr,
								const NkTrainState *state = nullptr);

			// Recharge les poids (mêmes nombre/formes que lors de la sauvegarde) dans des
			// paramètres déjà construits, via SetValue. false si fichier illisible/incompatible.
			bool LoadCheckpointWeights(const char *path, NkVector<NkVar> &params);

			// Recharge l'état Adam (moments + pas) DANS `opt` (déjà construit avec les mêmes
			// `params`, donc le même nombre de tenseurs). `params` sert à valider les formes.
			// false si le fichier ne contient pas de bloc optimiseur (hasOpt=0) ou est incompatible.
			bool LoadCheckpointOptState(const char *path, const NkVector<NkVar> &params, optim::NkAdam &opt);

			// Recharge l'état de boucle générique (époque/pas/meilleure métrique/patience).
			// false si le fichier ne contient pas ce bloc (hasState=0) ou est illisible.
			bool LoadCheckpointTrainState(const char *path, NkTrainState &state);

			// true si le fichier contient un état d'optimiseur exploitable (lecture rapide,
			// n'alloue pas les tenseurs). Utile pour décider Prepare-from-scratch vs reprise.
			bool CheckpointHasOptState(const char *path);

		} // namespace train
	} // namespace ai
} // namespace nkentseu
