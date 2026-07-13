// =============================================================================
// NkRnn.h — cellules récurrentes GRU / LSTM (NKAI, Phase 2).
//
// Cellules récurrentes construites UNIQUEMENT sur l'autograd (Matmul/Add/Mul/
// Sigmoid/Tanh) via des couches NkDense pour chaque porte — donc entièrement
// différentiables et entraînables par NKOptim. Chaque porte a un poids d'ENTRÉE
// (x) et un poids d'état CACHÉ (h), à la manière de PyTorch (b_ih + b_hh).
//
//   GRU  :  r = σ(Wxr·x + Whr·h)            (porte de réinitialisation)
//           z = σ(Wxz·x + Whz·h)            (porte de mise à jour)
//           n = tanh(Wxn·x + r ⊙ (Whn·h))   (candidat)
//           h'= (1−z) ⊙ n + z ⊙ h
//
//   LSTM :  i = σ(Wxi·x + Whi·h)  f = σ(Wxf·x + Whf·h)
//           g = tanh(Wxg·x + Whg·h)  o = σ(Wxo·x + Who·h)
//           c'= f ⊙ c + i ⊙ g       h'= o ⊙ tanh(c')
//
// Convention de forme : x = [B, in], h = c = [B, H]. Le déroulé dans le temps
// prend/rend une LISTE de pas (NkVector<NkVar>), et `StackTime` les empile en un
// tenseur [T, B, H] (via Concat0) — prêt pour la perte CTC. Namespace nn.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKNN/NkDense.h"
#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace nn {

			// État d'une LSTM : (h, c). Le GRU n'a qu'un état caché h.
			struct NkLSTMState {
					NkVar h;
					NkVar c;
			};

			// ---- Cellule GRU -------------------------------------------------
			class NkGRUCell {
				public:
					NkGRUCell() = default;
					NkGRUCell(uint32 inFeatures, uint32 hidden, uint32 seed = 1u);

					// Un pas : x = [B, in], h = [B, H] -> nouvel état caché [B, H].
					NkVar Forward(const NkVar &x, const NkVar &h) const;

					uint32 Hidden() const {
						return mHidden;
					}
					void Parameters(NkVector<NkVar> &out) const;

				private:
					uint32 mHidden = 0;
					NkDense mXr, mHr; // porte réinitialisation (entrée, caché)
					NkDense mXz, mHz; // porte mise à jour
					NkDense mXn, mHn; // candidat
			};

			// ---- Cellule LSTM ------------------------------------------------
			class NkLSTMCell {
				public:
					NkLSTMCell() = default;
					NkLSTMCell(uint32 inFeatures, uint32 hidden, uint32 seed = 1u);

					// Un pas : x = [B, in], état (h, c) = [B, H] -> nouvel état (h', c').
					NkLSTMState Forward(const NkVar &x, const NkLSTMState &s) const;

					uint32 Hidden() const {
						return mHidden;
					}
					void Parameters(NkVector<NkVar> &out) const;

				private:
					uint32 mHidden = 0;
					NkDense mXi, mHi; // porte d'entrée
					NkDense mXf, mHf; // porte d'oubli
					NkDense mXg, mHg; // candidat de cellule
					NkDense mXo, mHo; // porte de sortie
			};

			// ---- Utilitaires de séquence -------------------------------------
			// État caché initial nul [B, H] (feuille non entraînable).
			NkVar ZeroState(uint32 batch, uint32 hidden);

			// Déroule un GRU sur une séquence `xs` (liste de [B, in]) depuis h0 [B, H].
			// Renvoie la liste des états cachés à chaque pas ([B, H]).
			NkVector<NkVar> GRURunSeq(const NkGRUCell &cell, const NkVector<NkVar> &xs, const NkVar &h0);

			// Idem pour LSTM (h0, c0). Renvoie la liste des h à chaque pas.
			NkVector<NkVar> LSTMRunSeq(const NkLSTMCell &cell, const NkVector<NkVar> &xs, const NkLSTMState &s0);

			// Empile une liste de T tenseurs [B, H] en un seul tenseur [T, B, H]
			// (chaque pas remodelé en [1, B, H] puis concaténé sur l'axe 0). Différentiable.
			NkVar StackTime(const NkVector<NkVar> &steps);

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
