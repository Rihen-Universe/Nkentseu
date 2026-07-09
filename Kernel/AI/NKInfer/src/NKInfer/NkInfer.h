// =============================================================================
// NkInfer.h — inférence + persistance des poids (NKAI, Phase 3).
//
// Sérialise/recharge les PARAMÈTRES d'un modèle (liste de feuilles NkVar) dans un
// fichier binaire simple, et fournit la prédiction (argmax des logits). Le modèle
// lui-même reste défini par son forward (callable) : on ne sérialise que les
// tenseurs de poids, rechargés en place via NkVar::SetValue. Namespace :
// nkentseu::ai::infer.
//
// Format « NKMD » : ['N','K','M','D'] | version u32 | count u32 | pour chaque
// tenseur : rank u32, dims[rank] i64, data[numel] f32 (row-major, natif).
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Sauve les valeurs des paramètres dans `path`. true si OK.
			bool SaveParams(const char *path, const NkVector<NkVar> &params);

			// Recharge les poids depuis `path` DANS des paramètres déjà construits
			// (mêmes nombre et formes que lors de la sauvegarde) via SetValue.
			// Renvoie false si le fichier est illisible ou incompatible.
			bool LoadParams(const char *path, NkVector<NkVar> &params);

			// Prédiction : argmax par ligne de logits [B,C] -> étiquettes [B].
			NkVector<int32> Predict(const NkTensor &logits);

			// Étiquette prédite pour un unique vecteur de logits [C] ou [1,C].
			int32 PredictOne(const NkTensor &logits);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
