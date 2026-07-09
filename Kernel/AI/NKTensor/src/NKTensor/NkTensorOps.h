// =============================================================================
// NkTensorOps.h — opérations sur tenseurs CPU (NKAI, Jalon 1/2).
//
// Élémentaires (+ broadcasting), unaires, produit de matrices, réductions.
// Toutes renvoient un NOUVEAU tenseur contigu (sauf mention). dtype des sorties
// = dtype des entrées ; les deux opérandes binaires doivent partager le dtype.
// =============================================================================
#pragma once

#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace ops {

			// ---- Binaires élémentaires (broadcasting numpy-style) --------------
			NkTensor Add(const NkTensor &a, const NkTensor &b);
			NkTensor Sub(const NkTensor &a, const NkTensor &b);
			NkTensor Mul(const NkTensor &a, const NkTensor &b);
			NkTensor Div(const NkTensor &a, const NkTensor &b);

			// ---- Binaires avec scalaire ---------------------------------------
			NkTensor AddScalar(const NkTensor &a, double s);
			NkTensor MulScalar(const NkTensor &a, double s);

			// ---- Unaires -------------------------------------------------------
			NkTensor Neg(const NkTensor &a);
			NkTensor Abs(const NkTensor &a);
			NkTensor Exp(const NkTensor &a);  // flottants
			NkTensor Sqrt(const NkTensor &a); // flottants
			NkTensor Relu(const NkTensor &a);
			NkTensor Sigmoid(const NkTensor &a);
			NkTensor Tanh(const NkTensor &a);

			// ---- Algèbre linéaire ---------------------------------------------
			// Produit de matrices 2D : (M,K) x (K,N) -> (M,N). Flottants.
			NkTensor Matmul(const NkTensor &a, const NkTensor &b);

			// ---- Réductions ----------------------------------------------------
			// Globales -> tenseur scalaire de forme {1}.
			NkTensor Sum(const NkTensor &a);
			NkTensor Mean(const NkTensor &a);
			NkTensor Max(const NkTensor &a);

			// Le long d'un axe (keepdim=false -> l'axe disparaît).
			NkTensor Sum(const NkTensor &a, uint32 axis);
			NkTensor Mean(const NkTensor &a, uint32 axis);
			NkTensor Max(const NkTensor &a, uint32 axis);
			NkTensor Argmax(const NkTensor &a, uint32 axis); // sortie i64

			// ---- Comparaison ---------------------------------------------------
			// Vrai si mêmes formes et |a-b| <= eps élément par élément.
			bool AllClose(const NkTensor &a, const NkTensor &b, double eps = 1e-5);

		} // namespace ops
	} // namespace ai
} // namespace nkentseu
