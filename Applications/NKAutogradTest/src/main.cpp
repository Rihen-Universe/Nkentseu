// =============================================================================
// NKAutogradTest — preuve du moteur d'autodiff (NKAI, Phase 2).
//
//   1) Vérifie les gradients de chaque op (Mul, Matmul, Tanh, Sigmoid, ReLU, MSE)
//      contre des DIFFÉRENCES FINIES centrées (dL/dx ≈ (L(x+ε)-L(x-ε))/2ε).
//   2) Entraîne un MLP 2-8-1 sur XOR (tanh caché + sigmoid sortie + MSE, SGD
//      manuel) — construit UNIQUEMENT avec l'autograd. La perte doit chuter et
//      les 4 prédictions doivent être correctes.
//   3) ÉQUIVALENCE CPU <-> GPU du trio NkLlamaLM (RMSNorm, RoPE, SwiGLU), avant
//      ET arrière : le chemin CPU sert d'oracle aux noyaux qui le remplacent.
// =============================================================================
#include "NKAutograd/NkVar.h"
#include "NKNN/NkLlama.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

// dL/dx analytique (autograd) vs numérique (différences finies centrées).
template <typename F> static void GradCheck(const char *name, const NkTensor &x0, F buildLoss) {
	// Gradient analytique.
	NkVar x = NkVar::Leaf(x0.Clone(), true);
	NkVar loss = buildLoss(x);
	loss.Backward();
	NkTensor ga = x.Grad().Contiguous();
	const float *gp = ga.DataAs<float>();

	// Gradient numérique élément par élément.
	NkTensor xc = x0.Contiguous().Clone();
	float *xp = xc.DataAs<float>();
	const int64 n = NkShapeNumel(xc.Shape());
	const float eps = 1e-3f;
	double maxErr = 0.0;
	for (int64 i = 0; i < n; ++i) {
		const float o = xp[i];
		xp[i] = o + eps;
		double Lp = buildLoss(NkVar::Leaf(xc.Clone(), false)).Value().GetItem(NkShape{(int64)0});
		xp[i] = o - eps;
		double Lm = buildLoss(NkVar::Leaf(xc.Clone(), false)).Value().GetItem(NkShape{(int64)0});
		xp[i] = o;
		const double num = (Lp - Lm) / (2.0 * (double)eps);
		const double err = std::fabs(num - (double)gp[i]);
		if (err > maxErr)
			maxErr = err;
	}
	const bool ok = maxErr < 1e-2;
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %-18s  err max vs diff. finies = %.2e\n", ok ? "OK" : "KO", name, maxErr);
}

static NkTensor Mat(const NkShape &shape, const float *data) {
	return NkTensor::FromData(shape, data, NkDType::NK_F32);
}

int main() {
	printf("=== NKAutogradTest : gradients (diff. finies) + XOR ===\n\n");

	printf("-- Vérification des gradients --\n");

	// 1) Sum(x ⊙ b) : dL/dx = b.
	{
		float bd[4] = {0.5f, -1.0f, 2.0f, 0.25f};
		float xd[4] = {1.0f, 2.0f, -1.0f, 3.0f};
		NkTensor b = Mat(NkShape{4}, bd);
		GradCheck("Mul+Sum", Mat(NkShape{4}, xd),
				  [b](NkVar x) { return autograd::Sum(autograd::Mul(x, NkVar::Leaf(b, false))); });
	}
	// 2) Sum(tanh(x)).
	{
		float xd[5] = {-1.5f, -0.3f, 0.0f, 0.7f, 2.0f};
		GradCheck("Tanh+Sum", Mat(NkShape{5}, xd), [](NkVar x) { return autograd::Sum(autograd::Tanh(x)); });
	}
	// 3) Sum(sigmoid(x)).
	{
		float xd[5] = {-2.0f, -0.5f, 0.1f, 1.0f, 3.0f};
		GradCheck("Sigmoid+Sum", Mat(NkShape{5}, xd), [](NkVar x) { return autograd::Sum(autograd::Sigmoid(x)); });
	}
	// 4) Sum(relu(x)) — x loin de 0 (dérivée non définie en 0).
	{
		float xd[5] = {-2.0f, -0.4f, 0.6f, 1.3f, 2.5f};
		GradCheck("Relu+Sum", Mat(NkShape{5}, xd), [](NkVar x) { return autograd::Sum(autograd::Relu(x)); });
	}
	// 5) Sum(A · B) : gradient par rapport à A.
	{
		float ad[6] = {1, 2, 3, 4, 5, 6};			  // [2,3]
		float bd[6] = {0.5f, 1, -1, 2, 0.25f, -0.5f}; // [3,2]
		NkTensor B = Mat(NkShape{3, 2}, bd);
		GradCheck("Matmul+Sum", Mat(NkShape{2, 3}, ad),
				  [B](NkVar A) { return autograd::Sum(autograd::Matmul(A, NkVar::Leaf(B, false))); });
	}
	// 5bis) Matmul par LOTS : Sum(A · B) sur [2,2,3]·[2,3,2] -> dA et dB.
	{
		float ad[12];
		for (int i = 0; i < 12; i++)
			ad[i] = (float)(i + 1) * 0.1f; // [2,2,3]
		float bd[12];
		for (int i = 0; i < 12; i++)
			bd[i] = (float)(i % 5 - 2) * 0.3f; // [2,3,2]
		NkTensor Bb = Mat(NkShape{2, 3, 2}, bd);
		GradCheck("BMatmul dA", Mat(NkShape{2, 2, 3}, ad),
				  [Bb](NkVar A) { return autograd::Sum(autograd::Matmul(A, NkVar::Leaf(Bb, false))); });
		NkTensor Aa = Mat(NkShape{2, 2, 3}, ad);
		GradCheck("BMatmul dB", Mat(NkShape{2, 3, 2}, bd),
				  [Aa](NkVar B) { return autograd::Sum(autograd::Matmul(NkVar::Leaf(Aa, false), B)); });
	}
	// 5ter) LayerNorm sur le dernier axe : Sum(LayerNorm(x)).
	{
		float xd[8] = {1, 3, 2, 5, -1, 0, 4, 2}; // [2,4]
		GradCheck("LayerNorm", Mat(NkShape{2, 4}, xd), [](NkVar x) { return autograd::Sum(autograd::LayerNorm(x)); });
	}
	// 5quater) Softmax (dernier axe) : Sum(softmax(x) ⊙ W) -> gradient non trivial.
	{
		float xd[8] = {1, 2, 0, -1, 0.5f, 1.5f, -0.5f, 2}; // [2,4]
		float wd[8] = {0.3f, -0.2f, 0.5f, 0.1f, -0.4f, 0.2f, 0.6f, -0.1f};
		NkTensor W = Mat(NkShape{2, 4}, wd);
		GradCheck("Softmax", Mat(NkShape{2, 4}, xd),
				  [W](NkVar x) { return autograd::Sum(autograd::Mul(autograd::Softmax(x), NkVar::Leaf(W, false))); });
		// Softmax CAUSAL sur [2,2] (T=2) : la requête i ne voit que les clés j<=i.
		float sd[4] = {0.5f, -1, 2, 0.3f}; // scores [2,2]
		float w2[4] = {0.4f, 0.2f, -0.3f, 0.5f};
		NkTensor W2 = Mat(NkShape{2, 2}, w2);
		GradCheck("SoftmaxCausal", Mat(NkShape{2, 2}, sd), [W2](NkVar x) {
			return autograd::Sum(autograd::Mul(autograd::SoftmaxCausal(x), NkVar::Leaf(W2, false)));
		});
	}
	// 5quinquies) GELU + Embedding.
	{
		float xd[6] = {-2, -0.5f, 0, 0.5f, 1, 2};
		GradCheck("GELU", Mat(NkShape{6}, xd), [](NkVar x) { return autograd::Sum(autograd::Gelu(x)); });
		// Embedding : gradient p/r à la TABLE [vocab=3, d=2] ; indices {0,1,2,1}.
		float idd[4] = {0, 1, 2, 1};
		NkTensor idx = Mat(NkShape{4}, idd);
		float wd[8] = {0.2f, -0.3f, 0.5f, 0.1f, -0.4f, 0.6f, 0.3f, -0.2f}; // [4,2] poids
		NkTensor W = Mat(NkShape{4, 2}, wd);
		GradCheck("Embedding", Mat(NkShape{3, 2}, wd), // table init (réutilise wd, 6 val)
				  [idx, W](NkVar table) {
					  return autograd::Sum(autograd::Mul(autograd::Embedding(table, idx), NkVar::Leaf(W, false)));
				  });
	}
	// 5sexies) Briques des transformeurs modernes : RMSNorm, SwiGLU, RoPE.
	// Ces trois-la existaient deja en INFERENCE (NKInfer/NkQwen2Block) mais leur
	// derivee n'y couvrait que des adaptateurs LoRA sur un socle GELE — rien pour
	// entrainer depuis zero. Ecrites ici comme vraies operations autograd, leur
	// seule preuve valable est la confrontation aux differences finies.
	{
		// RMSNorm : y = x/sqrt(moyenne(x^2)+eps) sur le dernier axe. On multiplie
		// par des poids fixes avant de sommer : la somme de y seule serait presque
		// insensible a x et le test ne verifierait rien.
		float xd[8] = {0.7f, -1.3f, 0.2f, 2.1f, -0.5f, 0.9f, 1.4f, -0.8f};
		float wd2[8] = {0.5f, -0.2f, 1.1f, 0.3f, -0.7f, 0.4f, 0.9f, -0.6f};
		NkTensor Wr = Mat(NkShape{2, 4}, wd2);
		GradCheck("RMSNorm", Mat(NkShape{2, 4}, xd), [Wr](NkVar x) {
			return autograd::Sum(autograd::Mul(autograd::RMSNorm(x), NkVar::Leaf(Wr, false)));
		});

		// SwiGLU : gradient p/r a la PORTE (le plus delicat : il depend aussi de u).
		float gd[6] = {-1.5f, -0.3f, 0.0f, 0.4f, 1.2f, 2.5f};
		float ud[6] = {0.6f, -1.1f, 0.8f, 0.2f, -0.9f, 1.3f};
		NkTensor U = Mat(NkShape{6}, ud);
		GradCheck("SwiGLU dGate", Mat(NkShape{6}, gd),
				  [U](NkVar gate) { return autograd::Sum(autograd::SwiGLU(gate, NkVar::Leaf(U, false))); });
		NkTensor G = Mat(NkShape{6}, gd);
		GradCheck("SwiGLU dUp", Mat(NkShape{6}, ud),
				  [G](NkVar up) { return autograd::Sum(autograd::SwiGLU(NkVar::Leaf(G, false), up)); });

		// RoPE : [T=3, hd=4]. Somme PONDEREE : une rotation conserve la somme des
		// carres, pas la somme simple, mais un poids uniforme rendrait le gradient
		// trop regulier pour reveler une erreur d'indice.
		float rd[12] = {0.3f, -0.7f, 1.2f, 0.5f, -1.1f, 0.8f, 0.2f, -0.4f, 0.9f, 1.5f, -0.6f, 0.1f};
		float rw[12] = {1.0f, 0.4f, -0.8f, 0.6f, 0.2f, -1.2f, 0.7f, 0.9f, -0.3f, 0.5f, 1.1f, -0.5f};
		NkTensor Wp = Mat(NkShape{3, 4}, rw);
		GradCheck("RoPE", Mat(NkShape{3, 4}, rd), [Wp](NkVar x) {
			return autograd::Sum(autograd::Mul(autograd::RoPE(x, 0, 10000.0), NkVar::Leaf(Wp, false)));
		});
		GradCheck("RoPE decalee", Mat(NkShape{3, 4}, rd), [Wp](NkVar x) {
			return autograd::Sum(autograd::Mul(autograd::RoPE(x, 7, 10000.0), NkVar::Leaf(Wp, false)));
		});
	}
	// 5septies) RoPE : la PROPRIETE qu'aucune difference finie ne verifie. Une
	// rotation conserve les longueurs, et le produit scalaire entre deux positions
	// ne doit dependre que de leur ECART — c'est tout l'interet de RoPE face a des
	// positions apprises. On le constate au lieu de le supposer.
	{
		float ad[4] = {0.6f, -0.9f, 1.4f, 0.3f};
		NkTensor x1 = Mat(NkShape{1, 4}, ad);
		auto prodScalaire = [&](int p, int q) {
			NkVar u = autograd::RoPE(NkVar::Leaf(x1, false), p, 10000.0);
			NkVar v = autograd::RoPE(NkVar::Leaf(x1, false), q, 10000.0);
			return autograd::Sum(autograd::Mul(u, v)).Value().GetItem(NkShape{(int64)0});
		};
		const double d02 = prodScalaire(0, 2);
		const double d57 = prodScalaire(5, 7); // meme ecart -> meme produit scalaire
		const double norme0 = prodScalaire(0, 0);
		double n2 = 0;
		for (int i = 0; i < 4; ++i)
			n2 += (double)ad[i] * (double)ad[i];
		const bool ecartSeul = std::fabs(d02 - d57) < 1e-4;
		const bool normeGardee = std::fabs(norme0 - n2) < 1e-4;
		(ecartSeul ? g_pass : g_fail)++;
		printf("  [ %s ] %-18s  <p=0,q=2> = %.6f  vs  <p=5,q=7> = %.6f\n", ecartSeul ? "OK" : "KO",
			   "RoPE ecart seul", d02, d57);
		(normeGardee ? g_pass : g_fail)++;
		printf("  [ %s ] %-18s  norme apres rotation = %.6f  (avant %.6f)\n", normeGardee ? "OK" : "KO",
			   "RoPE norme gardee", norme0, n2);
	}
	// 6) MSE(pred, cible) : dL/dpred = 2(pred-cible)/N.
	{
		float pd[4] = {0.2f, 0.9f, 0.7f, 0.1f};
		float td[4] = {0.0f, 1.0f, 1.0f, 0.0f};
		NkTensor t = Mat(NkShape{4}, td);
		GradCheck("MSE", Mat(NkShape{4}, pd), [t](NkVar p) { return autograd::MSE(p, NkVar::Leaf(t, false)); });
	}
	// 7) SoftmaxCrossEntropy(logits[B,C], onehot) : dLogits = (softmax-onehot)/B.
	{
		float ld[6] = {2.0f, 0.5f, -1.0f, 0.1f, 1.5f, 0.3f}; // [2,3] logits
		float oh[6] = {1, 0, 0, 0, 1, 0};					 // classes 0 et 1
		NkTensor t = Mat(NkShape{2, 3}, oh);
		GradCheck("SoftmaxCE", Mat(NkShape{2, 3}, ld),
				  [t](NkVar z) { return autograd::SoftmaxCrossEntropy(z, NkVar::Leaf(t, false)); });
	}
	// 7bis) SoftmaxCrossEntropyIndexed : MÊME résultat via des indices [B] au lieu du one-hot [B,C].
	{
		float ld[6] = {2.0f, 0.5f, -1.0f, 0.1f, 1.5f, 0.3f}; // [2,3] logits
		float idx[2] = {0.f, 1.f};							 // mêmes classes que le one-hot ci-dessus
		NkTensor ti = Mat(NkShape{2}, idx);
		GradCheck("SoftmaxCE_Idx", Mat(NkShape{2, 3}, ld),
				  [ti](NkVar z) { return autograd::SoftmaxCrossEntropyIndexed(z, NkVar::Leaf(ti, false)); });
	}
	// 7ter) Cible-indices avec ligne MASQUÉE (idx<0) : gradient NUL sur la ligne masquée.
	{
		float ld[6] = {2.0f, 0.5f, -1.0f, 0.1f, 1.5f, 0.3f};
		float idx[2] = {-1.f, 1.f}; // ligne 0 masquée, ligne 1 = classe 1
		NkTensor ti = Mat(NkShape{2}, idx);
		GradCheck("SoftmaxCE_IdxMask", Mat(NkShape{2, 3}, ld),
				  [ti](NkVar z) { return autograd::SoftmaxCrossEntropyIndexed(z, NkVar::Leaf(ti, false)); });
	}
	// 8) Conv2D — gradient p/r à l'ENTRÉE : input [1,1,4,4] ⊛ weight [1,1,2,2].
	{
		float xd[16] = {1, 2, 0, 1, 0, 1, 3, 2, 2, 0, 1, 1, 1, 1, 0, 2};
		float wd[4] = {1, -1, 0.5f, 2};
		NkTensor Wt = Mat(NkShape{1, 1, 2, 2}, wd);
		GradCheck("Conv2D dX", Mat(NkShape{1, 1, 4, 4}, xd),
				  [Wt](NkVar x) { return autograd::Sum(autograd::Conv2D(x, NkVar::Leaf(Wt, false), 1, 0)); });
	}
	// 9) Conv2D — gradient p/r aux POIDS.
	{
		float xd[16] = {1, 2, 0, 1, 0, 1, 3, 2, 2, 0, 1, 1, 1, 1, 0, 2};
		float wd[4] = {1, -1, 0.5f, 2};
		NkTensor Xc = Mat(NkShape{1, 1, 4, 4}, xd);
		GradCheck("Conv2D dW", Mat(NkShape{1, 1, 2, 2}, wd),
				  [Xc](NkVar w) { return autograd::Sum(autograd::Conv2D(NkVar::Leaf(Xc, false), w, 1, 0)); });
	}
	// 10) MaxPool2D — gradient routé vers l'argmax (maxima UNIQUES par fenêtre :
	//     les diff. finies sont invalides sur une égalité, l'argmax basculant).
	{
		float xd[16] = {1, 3, 2, 9, 4, 1, 7, 2, 0, 5, 1, 1, 2, 0, 3, 6};
		GradCheck("MaxPool2D", Mat(NkShape{1, 1, 4, 4}, xd),
				  [](NkVar x) { return autograd::Sum(autograd::MaxPool2D(x, 2, 2)); });
	}
	// 11) ConvTranspose2D — gradient p/r à l'ENTRÉE : input [1,1,2,2], weight [1,1,2,2].
	{
		float xd[4] = {1, 2, 3, 4};
		float wd[4] = {1, -1, 0.5f, 2};
		NkTensor Wt = Mat(NkShape{1, 1, 2, 2}, wd);
		GradCheck("ConvT2D dX", Mat(NkShape{1, 1, 2, 2}, xd),
				  [Wt](NkVar x) { return autograd::Sum(autograd::ConvTranspose2D(x, NkVar::Leaf(Wt, false), 1, 0)); });
	}
	// 12) ConvTranspose2D — gradient p/r aux POIDS.
	{
		float xd[4] = {1, 2, 3, 4};
		float wd[4] = {1, -1, 0.5f, 2};
		NkTensor Xc = Mat(NkShape{1, 1, 2, 2}, xd);
		GradCheck("ConvT2D dW", Mat(NkShape{1, 1, 2, 2}, wd),
				  [Xc](NkVar w) { return autograd::Sum(autograd::ConvTranspose2D(NkVar::Leaf(Xc, false), w, 1, 0)); });
	}
	// 13) Exp — d/dx exp(x) = exp(x).
	{
		float xd[4] = {-1.f, 0.f, 0.5f, 1.2f};
		GradCheck("Exp", Mat(NkShape{4}, xd), [](NkVar x) { return autograd::Sum(autograd::Exp(x)); });
	}
	// 14-15) Conv3D — gradients p/r entrée et poids : [1,1,3,3,3] ⊛ [1,1,2,2,2].
	{
		float xd[27] = {1, 2, 0, 1, 0, 3, 2, 1, 0, 0, 1, 2, 3, 0, 1, 1, 2, 0, 2, 0, 1, 0, 3, 2, 1, 0, 1};
		float wd[8] = {1, -1, 0.5f, 2, -0.5f, 1, 0, 1.5f};
		NkTensor Wt = Mat(NkShape{1, 1, 2, 2, 2}, wd);
		GradCheck("Conv3D dX", Mat(NkShape{1, 1, 3, 3, 3}, xd),
				  [Wt](NkVar x) { return autograd::Sum(autograd::Conv3D(x, NkVar::Leaf(Wt, false), 1, 0)); });
		NkTensor Xc = Mat(NkShape{1, 1, 3, 3, 3}, xd);
		GradCheck("Conv3D dW", Mat(NkShape{1, 1, 2, 2, 2}, wd),
				  [Xc](NkVar w) { return autograd::Sum(autograd::Conv3D(NkVar::Leaf(Xc, false), w, 1, 0)); });
	}
	// 16-17) ConvTranspose3D — gradients : [1,1,2,2,2] -> upsample.
	{
		float xd[8] = {1, 2, 3, 4, 0, 1, 2, 1};
		float wd[8] = {1, -1, 0.5f, 2, -0.5f, 1, 0, 1.5f};
		NkTensor Wt = Mat(NkShape{1, 1, 2, 2, 2}, wd);
		GradCheck("ConvT3D dX", Mat(NkShape{1, 1, 2, 2, 2}, xd),
				  [Wt](NkVar x) { return autograd::Sum(autograd::ConvTranspose3D(x, NkVar::Leaf(Wt, false), 1, 0)); });
		NkTensor Xc = Mat(NkShape{1, 1, 2, 2, 2}, xd);
		GradCheck("ConvT3D dW", Mat(NkShape{1, 1, 2, 2, 2}, wd),
				  [Xc](NkVar w) { return autograd::Sum(autograd::ConvTranspose3D(NkVar::Leaf(Xc, false), w, 1, 0)); });
	}
	// 18) SigmoidBCE — dLogits = (sigmoid(logits) - cible)/N.
	{
		float ld[6] = {-1.5f, 0.3f, 2.0f, -0.7f, 1.1f, 0.0f};
		float td[6] = {0.f, 1.f, 1.f, 0.f, 1.f, 0.f};
		NkTensor t = Mat(NkShape{6}, td);
		GradCheck("SigmoidBCE", Mat(NkShape{6}, ld),
				  [t](NkVar z) { return autograd::SigmoidBCE(z, NkVar::Leaf(t, false)); });
	}
	// 19) Upsample2x — chaque entrée répétée 4× (gradient = 4).
	{
		float xd[4] = {1, 2, 3, 4};
		GradCheck("Upsample2x", Mat(NkShape{1, 1, 2, 2}, xd),
				  [](NkVar x) { return autograd::Sum(autograd::Upsample2x(x)); });
	}
	// 20) Concat0 — empile [2,3] ⊕ constante [1,3] -> [3,3], pondéré puis sommé.
	//     Vérifie que le gradient est redécoupé correctement vers le 1er opérande.
	{
		float xd[6] = {1, 2, 3, 4, 5, 6};	 // [2,3]
		float cd[3] = {0.5f, -1.f, 2.f};	 // constante [1,3]
		float wd[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}; // poids [3,3]
		NkTensor C = Mat(NkShape{1, 3}, cd);
		NkTensor W = Mat(NkShape{3, 3}, wd);
		GradCheck("Concat0", Mat(NkShape{2, 3}, xd), [C, W](NkVar x) {
			NkVar cat = autograd::Concat0(x, NkVar::Leaf(C, false)); // [3,3]
			return autograd::Sum(autograd::Mul(cat, NkVar::Leaf(W, false)));
		});
	}
	// 21) CTCLoss — logits [T=4,B=1,V=3], cible = {0,1} (blanc = 2). Vérifie le gradient
	//     analytique (forward-backward) contre les différences finies.
	{
		float ld[12] = {0.2f, -0.3f, 0.1f, 0.5f, 0.4f, -0.2f, -0.1f, 0.6f, 0.0f, 0.3f, -0.5f, 0.2f};
		NkVector<NkVector<int32>> tgts;
		tgts.Resize(1);
		tgts[0].PushBack(0);
		tgts[0].PushBack(1);
		GradCheck("CTCLoss", Mat(NkShape{4, 1, 3}, ld),
				  [tgts](NkVar z) { return autograd::CTCLoss(z, tgts, /*blank*/ 2); });
	}
	// 22) CTCLoss cible vide — seul le blanc participe (tous les pas -> blanc).
	{
		float ld[8] = {0.2f, -0.3f, 0.5f, 0.4f, -0.1f, 0.6f, 0.3f, -0.5f}; // [T=4,B=1,V=2]
		NkVector<NkVector<int32>> tgts;
		tgts.Resize(1); // cible vide pour l'exemple 0
		GradCheck("CTCLoss vide", Mat(NkShape{4, 1, 2}, ld),
				  [tgts](NkVar z) { return autograd::CTCLoss(z, tgts, /*blank*/ 1); });
	}

	// -----------------------------------------------------------------------
	// ACCUMULATION DE GRADIENT — deux Backward() successifs sur la MEME feuille.
	//
	// POURQUOI CE CAS MANQUAIT, ET POURQUOI IL COMPTE :
	// les cas ci-dessus font tous UN SEUL Backward() sur une feuille NEUVE.
	// Aucun ne couvre le scenario reel de l'entraineur.
	//
	// `NkGptTrainer::Fit` appelle `adam.ZeroGrad()` UNE fois par pas, puis
	// `scaled.Backward()` UNE fois par micro-lot (boucle `for m < ACCUM`), en
	// reconstruisant le graphe sur les MEMES noeuds-feuilles de parametres.
	// TOUTE la semantique de `--accum` repose donc sur un seul fait : le second
	// Backward() doit AJOUTER au gradient du premier, pas l'ECRASER.
	//
	// Or `NkVar::Backward()` commence par
	//     for (i < order.Size()) order[i]->grad = ZerosCommeSur(order[i]->value);
	// et `CollectTopo` descend jusqu'aux feuilles : les parametres sont dans
	// `order`. Ce test mesure ce que le code fait vraiment.
	//
	// L = Sum(x . b) donne dL/dx = b. Deux passes identiques doivent donner 2b.
	// -----------------------------------------------------------------------
	printf("\n-- Accumulation de gradient (deux Backward() sur la meme feuille) --\n");
	{
		float bd[4] = {0.5f, -1.0f, 2.0f, 0.25f};
		float xd[4] = {1.0f, 2.0f, -1.0f, 3.0f};
		NkTensor b = Mat(NkShape{4}, bd);
		NkVar xa = NkVar::Leaf(Mat(NkShape{4}, xd), true);

		// Micro-lot 1.
		NkVar l1 = autograd::Sum(autograd::Mul(xa, NkVar::Leaf(b, false)));
		l1.Backward();
		NkTensor g1 = xa.Grad().Contiguous().Clone();

		// Micro-lot 2 : graphe RECONSTRUIT sur la meme feuille `xa`, comme le fait
		// l'entraineur a chaque tour de sa boucle d'accumulation.
		NkVar l2 = autograd::Sum(autograd::Mul(xa, NkVar::Leaf(b, false)));
		l2.Backward();
		NkTensor g2 = xa.Grad().Contiguous().Clone();

		const float *p1 = g1.DataAs<float>();
		const float *p2 = g2.DataAs<float>();

		// Deux hypotheses exclusives, mesurees toutes les deux : on ne conclut
		// pas « ecrase » d'un simple echec de « accumule ».
		double ecartSiAccumule = 0.0; // |g2 - 2*g1| -> 0 si l'accumulation marche
		double ecartSiEcrase = 0.0;	  // |g2 -   g1| -> 0 si le second ecrase le premier
		for (int64 i = 0; i < 4; ++i) {
			const double dAcc = std::fabs((double)p2[i] - 2.0 * (double)p1[i]);
			const double dEcr = std::fabs((double)p2[i] - (double)p1[i]);
			if (dAcc > ecartSiAccumule)
				ecartSiAccumule = dAcc;
			if (dEcr > ecartSiEcrase)
				ecartSiEcrase = dEcr;
		}

		printf("  g1 = [%.3f %.3f %.3f %.3f]   (attendu : b)\n", p1[0], p1[1], p1[2], p1[3]);
		printf("  g2 = [%.3f %.3f %.3f %.3f]   (attendu : 2b si accumulation)\n", p2[0], p2[1], p2[2], p2[3]);
		printf("  |g2 - 2*g1| = %.2e     |g2 - g1| = %.2e\n", ecartSiAccumule, ecartSiEcrase);

		const bool accumule = ecartSiAccumule < 1e-6;
		const bool ecrase = ecartSiEcrase < 1e-6;
		(accumule ? g_pass : g_fail)++;
		if (accumule) {
			printf("  [ OK ] le second Backward() AJOUTE au premier : `--accum` fait ce qu'il annonce.\n");
		} else if (ecrase) {
			printf("  [ KO ] le second Backward() ECRASE le premier. `--accum N` ne cumule RIEN :\n");
			printf("         seul le DERNIER micro-lot contribue, et il reste divise par N.\n");
			printf("         => lot effectif ET taux d'apprentissage N fois plus petits qu'annonces.\n");
		} else {
			printf("  [ KO ] ni accumulation ni ecrasement franc — troisieme regime, a diagnostiquer.\n");
		}
	}

	// -----------------------------------------------------------------------
	// Entraînement XOR : MLP 2-8-1, tanh caché + sigmoid sortie + MSE, SGD.
	// -----------------------------------------------------------------------
	printf("\n-- Entraînement XOR (MLP 2-8-1, SGD via autograd) --\n");

	const float Xd[8] = {0, 0, 0, 1, 1, 0, 1, 1};
	const float Yd[4] = {0, 1, 1, 0};
	NkTensor X = Mat(NkShape{4, 2}, Xd);
	NkTensor Y = Mat(NkShape{4, 1}, Yd);

	const int H = 8;
	NkTensor W1 = NkTensor::Zeros(NkShape{2, H});
	NkTensor b1 = NkTensor::Zeros(NkShape{1, H});
	NkTensor W2 = NkTensor::Zeros(NkShape{H, 1});
	NkTensor b2 = NkTensor::Zeros(NkShape{1, 1});

	// Init pseudo-aléatoire (LCG déterministe) dans [-0.5, 0.5] pour briser la symétrie.
	uint32 seed = 12345u;
	auto rnd = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return ((float)((seed >> 8) & 0xFFFFu) / 65535.0f - 0.5f);
	};
	{
		float *p = W1.DataAs<float>();
		for (int64 i = 0; i < NkShapeNumel(W1.Shape()); ++i)
			p[i] = rnd();
	}
	{
		float *p = W2.DataAs<float>();
		for (int64 i = 0; i < NkShapeNumel(W2.Shape()); ++i)
			p[i] = rnd();
	}

	const float lr = 1.0f;
	const int epochs = 8000;
	for (int e = 0; e <= epochs; ++e) {
		NkVar w1 = NkVar::Leaf(W1, true), B1 = NkVar::Leaf(b1, true);
		NkVar w2 = NkVar::Leaf(W2, true), B2 = NkVar::Leaf(b2, true);
		NkVar xin = NkVar::Leaf(X, false), yt = NkVar::Leaf(Y, false);

		NkVar h = autograd::Tanh(autograd::Add(autograd::Matmul(xin, w1), B1));
		NkVar o = autograd::Sigmoid(autograd::Add(autograd::Matmul(h, w2), B2));
		NkVar loss = autograd::MSE(o, yt);

		loss.Backward();

		// SGD : param -= lr * grad.
		W1 = ops::Sub(W1, ops::MulScalar(w1.Grad(), lr));
		b1 = ops::Sub(b1, ops::MulScalar(B1.Grad(), lr));
		W2 = ops::Sub(W2, ops::MulScalar(w2.Grad(), lr));
		b2 = ops::Sub(b2, ops::MulScalar(B2.Grad(), lr));

		if (e % 1000 == 0)
			printf("  epoch %5d : perte = %.6f\n", e, loss.Value().GetItem(NkShape{(int64)0}));
	}

	// Prédiction finale.
	NkVar h = autograd::Tanh(
		autograd::Add(autograd::Matmul(NkVar::Leaf(X, false), NkVar::Leaf(W1, false)), NkVar::Leaf(b1, false)));
	NkVar o = autograd::Sigmoid(autograd::Add(autograd::Matmul(h, NkVar::Leaf(W2, false)), NkVar::Leaf(b2, false)));
	const NkTensor &pred = o.Value();
	int correct = 0;
	printf("\n  XOR : entrée -> prédiction (cible)\n");
	for (int i = 0; i < 4; ++i) {
		double p = pred.GetItem(NkShape{(int64)i, (int64)0});
		double target = Yd[i];
		bool ok = (p > 0.5) == (target > 0.5);
		correct += ok ? 1 : 0;
		printf("    [%d,%d] -> %.3f  (%.0f)  %s\n", (int)Xd[i * 2], (int)Xd[i * 2 + 1], p, target, ok ? "OK" : "KO");
	}
	const bool xorOk = (correct == 4);
	(xorOk ? g_pass : g_fail)++;
	printf("  [ %s ] XOR appris (%d/4 prédictions correctes)\n", xorOk ? "OK" : "KO", correct);

	// -----------------------------------------------------------------------
	// Mode sans-gradient (no_grad, façon torch.no_grad()) : le forward reste
	// correct mais AUCUN graphe n'est retenu -> le nombre de nœuds VIVANTS
	// (NkVarNode::LiveCount()) reste PLAT quelle que soit la profondeur du
	// calcul, alors qu'en mode normal la chaîne entière reste vivante tant que
	// le résultat final est tenu (mesure réelle, pas une estimation).
	// -----------------------------------------------------------------------
	logger.Info("-- Mode sans-gradient (no_grad) : compteur de noeuds vivants --");
	{
		const int64 depth = 40;
		NkTensor x0 = NkTensor::Full(NkShape{(int64)1000}, 1.0);
		const int64 base = NkVarNode::LiveCount();

		int64 liveWithGraph = base;
		{
			NkVar x = NkVar::Leaf(x0, true);
			NkVar h2 = x;
			for (int64 i = 0; i < depth; ++i)
				h2 = autograd::Relu(autograd::MulScalar(h2, 1.0));
			liveWithGraph = NkVarNode::LiveCount(); // chaîne ENTIÈRE retenue par h2 (jusqu'à x)
		}											 // h2 sort de portée ici -> toute la chaîne libérée
		const int64 afterGraph = NkVarNode::LiveCount();

		int64 liveNoGrad = base;
		{
			NkNoGradGuard guard; // désactive l'enregistrement du graphe pour ce bloc
			NkVar x = NkVar::Leaf(x0, true);
			NkVar h2 = x;
			for (int64 i = 0; i < depth; ++i)
				h2 = autograd::Relu(autograd::MulScalar(h2, 1.0));
			liveNoGrad = NkVarNode::LiveCount(); // SEULS x et h2 restent vivants, pas la chaîne
		}
		const int64 afterNoGrad = NkVarNode::LiveCount();

		const int64 growthWithGraph = liveWithGraph - base; // ~2*depth+1 (une op par appel)
		const int64 growthNoGrad = liveNoGrad - base;		 // reste petit (x + h2), PLAT malgré depth

		const bool noGradOk = (growthNoGrad <= 4) && (growthWithGraph >= depth) && (afterGraph == base) &&
							  (afterNoGrad == base);
		(noGradOk ? g_pass : g_fail)++;
		logger.Info("  [ {0} ] no_grad : croissance AVEC graphe = {1} noeuds vs SANS graphe (no_grad) = {2} noeuds "
					"(profondeur {3} ops) ; vivants avant={4} apres_normal={5} apres_nograd={6}",
					noGradOk ? "OK" : "KO", growthWithGraph, growthNoGrad, depth, base, afterGraph, afterNoGrad);
	}

	// -----------------------------------------------------------------------
	// Detach() (stop-gradient) : y=a*b est détaché avant d'entrer dans z=yd*c.
	// Le gradient DOIT être correct EN AVAL (dL/dc = y = 6) et ABSENT EN AMONT
	// du point de détachement (dL/da et dL/db jamais accumulés : a et b ne sont
	// même pas visités par le tri topologique du Backward).
	// -----------------------------------------------------------------------
	logger.Info("-- Detach() (stop-gradient) --");
	{
		float ad[1] = {2.0f}, bd[1] = {3.0f}, cd[1] = {4.0f};
		NkVar a = NkVar::Leaf(Mat(NkShape{1}, ad), true);
		NkVar b = NkVar::Leaf(Mat(NkShape{1}, bd), true);
		NkVar c = NkVar::Leaf(Mat(NkShape{1}, cd), true);

		NkVar y = autograd::Mul(a, b); // y = a*b = 6 (partie entraînable, PAS détachée elle-même)
		NkVar yd = y.Detach();			// coupe le graphe ICI : yd porte la valeur 6, sans parent
		NkVar z = autograd::Mul(yd, c); // z = yd*c = 24
		NkVar loss = autograd::Sum(z);
		loss.Backward();

		const bool gradAAbsent = !a.Grad().IsValid();
		const bool gradBAbsent = !b.Grad().IsValid();
		const double gradC = c.Grad().IsValid() ? c.Grad().GetItem(NkShape{(int64)0}) : -999.0;
		const bool downstreamOk = std::fabs(gradC - 6.0) < 1e-5; // dz/dc = yd.value = 6

		const bool detachOk = gradAAbsent && gradBAbsent && downstreamOk;
		(detachOk ? g_pass : g_fail)++;
		logger.Info("  [ {0} ] Detach : dL/dc = {1} (attendu 6.0, en aval du detach) ; dL/da absent = {2}, dL/db "
					"absent = {3} (en amont du detach)",
					detachOk ? "OK" : "KO", gradC, gradAAbsent, gradBAbsent);
	}

	// =========================================================================
	// ÉQUIVALENCE CPU <-> GPU pour le trio NkLlamaLM (RMSNorm, RoPE, SwiGLU).
	//
	// Ces trois opérations n'avaient AUCUN noyau GPU : elles redescendaient le
	// tenseur sur le processeur et le remontaient, en avant comme en arrière. Les
	// noyaux les remplacent, et le chemin CPU — déjà validé par l'entraînement —
	// devient leur ORACLE.
	//
	// Pourquoi ce test EXISTE : un noyau dont l'indexation est fausse NE PLANTE
	// PAS. Il dégrade en silence, et cela ne se verrait qu'à la courbe de perte
	// du grand run, des heures plus tard.
	//
	// TOLÉRANCE. Le chemin CPU accumule en double, le noyau en flottant simple :
	// l'écart attendu est celui de l'arrondi f32 (~6e-8 par opération), amplifié
	// par la longueur des accumulations. On exige 1e-5 en relatif, PAS l'égalité
	// au bit près — un seuil trop serré échouerait pour une raison étrangère à ce
	// qu'il prétend vérifier.
	{
		printf("\n--- Équivalence CPU/GPU : RMSNorm, RoPE, SwiGLU (avant ET arrière) ---\n");

		if (!NkTensorGpu::Get().IsAvailable()) {
			// Une absence de device ne doit PAS se lire comme un succès : sans GPU,
			// ce bloc n'a RIEN vérifié, et le compter comme réussi serait exactement
			// le défaut qu'on traque.
			++g_fail;
			printf("  [ KO ] aucun device GPU : les noyaux n'ont PAS pu être vérifiés\n");
		} else {
			uint64 rng = 0x2545F4914F6CDD1Dull; // déterministe : deux exécutions comparables
			auto frand = [&rng]() {
				rng = rng * 6364136223846793005ull + 1442695040888963407ull;
				return (float)((double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52) * 2.0 - 1.0);
			};
			auto randTensor = [&](const NkShape &s) {
				NkTensor t = NkTensor::Zeros(s);
				float *p = t.DataAs<float>();
				const int64 n = NkShapeNumel(s);
				for (int64 i = 0; i < n; ++i)
					p[i] = frand();
				return t;
			};
			auto compare = [](const char *quoi, const NkTensor &a, const NkTensor &b, double tol) {
				NkTensor ca = a.IsValid() ? a.ToCPU().Contiguous() : NkTensor{};
				NkTensor cb = b.IsValid() ? b.ToCPU().Contiguous() : NkTensor{};
				if (!ca.IsValid() || !cb.IsValid() || ca.Numel() != cb.Numel() || ca.Numel() == 0) {
					++g_fail;
					printf("  [ KO ] %-24s tenseur invalide ou tailles différentes -> RIEN mesuré\n", quoi);
					return;
				}
				const float *pa = ca.DataAs<float>();
				const float *pb = cb.DataAs<float>();
				double pire = 0.0, ampli = 0.0;
				for (int64 i = 0; i < ca.Numel(); ++i) {
					const double d = std::fabs((double)pa[i] - (double)pb[i]);
					if (d > pire)
						pire = d;
					const double m = std::fabs((double)pa[i]);
					if (m > ampli)
						ampli = m;
				}
				const double rel = (ampli > 0.0) ? pire / ampli : pire;
				const bool ok = rel < tol;
				(ok ? g_pass : g_fail)++;
				printf("  [ %s ] %-24s ecart relatif max = %.2e  (tol %.0e, %lld valeurs)\n", ok ? "OK" : "KO",
					   quoi, rel, tol, (long long)ca.Numel());
			};

			const double kTol = 1e-5;

			{ // RMSNorm : [rows, D]
				NkShape s;
				s.PushBack(24);
				s.PushBack(64);
				const NkTensor x = randTensor(s), w = randTensor(s);

				NkVar xc = NkVar::Leaf(x.Clone(), true);
				NkVar yc = autograd::RMSNorm(xc, 1e-6);
				autograd::Sum(autograd::Mul(yc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar xg = NkVar::Leaf(x.Clone().ToGPU(), true);
				NkVar yg = autograd::RMSNorm(xg, 1e-6);
				autograd::Sum(autograd::Mul(yg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("RMSNorm avant", yc.Value(), yg.Value(), kTol);
				compare("RMSNorm arriere", xc.Grad(), xg.Grad(), kTol);
			}

			{ // RoPE : [blocs, T, hd]
				NkShape s;
				s.PushBack(4);
				s.PushBack(32); // T
				s.PushBack(16); // hd (pair)
				const NkTensor x = randTensor(s), w = randTensor(s);

				NkVar xc = NkVar::Leaf(x.Clone(), true);
				NkVar yc = autograd::RoPE(xc, 0, 10000.0);
				autograd::Sum(autograd::Mul(yc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar xg = NkVar::Leaf(x.Clone().ToGPU(), true);
				NkVar yg = autograd::RoPE(xg, 0, 10000.0);
				autograd::Sum(autograd::Mul(yg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("RoPE avant", yc.Value(), yg.Value(), kTol);
				compare("RoPE arriere", xc.Grad(), xg.Grad(), kTol);

				// Contrôle INDÉPENDANT de l'oracle : la rotation conserve la norme.
				// Les deux chemins partagent la table cos/sin ; une table fausse leur
				// donnerait donc le MÊME mauvais résultat et la comparaison seule ne
				// verrait rien. Cette invariante, elle, ne dépend pas de la table.
				NkTensor a = x.Contiguous(), b = yg.Value().ToCPU().Contiguous();
				const float *pa = a.DataAs<float>();
				const float *pb = b.DataAs<float>();
				double na = 0.0, nb = 0.0;
				for (int64 i = 0; i < a.Numel(); ++i) {
					na += (double)pa[i] * (double)pa[i];
					nb += (double)pb[i] * (double)pb[i];
				}
				const double ecart = (na > 0.0) ? std::fabs(na - nb) / na : 1.0;
				const bool normeOk = ecart < 1e-5;
				(normeOk ? g_pass : g_fail)++;
				printf("  [ %s ] %-24s norme carree conservee a %.2e pres\n", normeOk ? "OK" : "KO",
					   "RoPE rotation pure", ecart);
			}

			{ // SwiGLU : deux entrées, deux gradients
				NkShape s;
				s.PushBack(16);
				s.PushBack(48);
				const NkTensor gt = randTensor(s), ut = randTensor(s), w = randTensor(s);

				NkVar gc = NkVar::Leaf(gt.Clone(), true), uc = NkVar::Leaf(ut.Clone(), true);
				NkVar hc = autograd::SwiGLU(gc, uc);
				autograd::Sum(autograd::Mul(hc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar gg = NkVar::Leaf(gt.Clone().ToGPU(), true), ug = NkVar::Leaf(ut.Clone().ToGPU(), true);
				NkVar hg = autograd::SwiGLU(gg, ug);
				autograd::Sum(autograd::Mul(hg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("SwiGLU avant", hc.Value(), hg.Value(), kTol);
				compare("SwiGLU arriere dG", gc.Grad(), gg.Grad(), kTol);
				compare("SwiGLU arriere dU", uc.Grad(), ug.Grad(), kTol);
			}

			// =================================================================
			// LES NOYAUX QUI EXISTAIENT DÉJÀ — jamais vérifiés jusqu'ici.
			//
			// LayerNorm, Gelu, SoftmaxCausal et Embedding ont des noyaux GPU
			// depuis des mois, sans aucun test d'équivalence. Le même défaut
			// muet peut y dormir. Ce n'est pas une précaution abstraite :
			// NkGPT était la RÉFÉRENCE de la course d'architecture, et un
			// noyau à moitié cassé de son côté aurait gonflé la victoire de
			// NkLlamaLM sans que rien ne le signale.
			// =================================================================
			{ // LayerNorm
				NkShape s;
				s.PushBack(24);
				s.PushBack(64);
				const NkTensor x = randTensor(s), w = randTensor(s);

				NkVar xc = NkVar::Leaf(x.Clone(), true);
				NkVar yc = autograd::LayerNorm(xc);
				autograd::Sum(autograd::Mul(yc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar xg = NkVar::Leaf(x.Clone().ToGPU(), true);
				NkVar yg = autograd::LayerNorm(xg);
				autograd::Sum(autograd::Mul(yg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("LayerNorm avant", yc.Value(), yg.Value(), kTol);
				compare("LayerNorm arriere", xc.Grad(), xg.Grad(), kTol);

				// Propriété intrinsèque, indépendante de l'oracle : après
				// normalisation, chaque ligne a une moyenne nulle et une variance
				// PRÉVISIBLE. Vrai même si les DEUX chemins se trompent.
				//
				// ⚠️ LA VARIANCE ATTENDUE N'EST PAS 1. LayerNorm calcule
				// y = (x−μ)/√(σ²+ε), donc Var(y) = σ²/(σ²+ε) — l'écart à 1 vaut
				// ε/σ², il est SYSTÉMATIQUE et calculable, pas accidentel. Une
				// première version comparait à 1 avec une tolérance élargie à 1e-3
				// pour l'absorber : elle se donnait un angle mort de la taille
				// exacte de l'epsilon, et toute vraie erreur de cette amplitude y
				// serait passée. On calcule donc l'attendu et on resserre au niveau
				// des autres contrôles.
				const double kEps = 1e-5; // celui du noyau (cf. kLayerNormFwdNkSL)
				NkTensor xx = x.Contiguous(), yy = yg.Value().ToCPU().Contiguous();
				const float *px = xx.DataAs<float>();
				const float *py = yy.DataAs<float>();
				double pireMoy = 0.0, pireVar = 0.0;
				for (int64 r = 0; r < 24; ++r) {
					// Variance de l'ENTRÉE : c'est elle qui fixe l'attendu.
					double mx = 0.0;
					for (int64 c = 0; c < 64; ++c)
						mx += (double)px[r * 64 + c];
					mx /= 64.0;
					double sx = 0.0;
					for (int64 c = 0; c < 64; ++c) {
						const double t = (double)px[r * 64 + c] - mx;
						sx += t * t;
					}
					sx /= 64.0;
					const double attendu = sx / (sx + kEps);

					double m = 0.0;
					for (int64 c = 0; c < 64; ++c)
						m += (double)py[r * 64 + c];
					m /= 64.0;
					double v = 0.0;
					for (int64 c = 0; c < 64; ++c) {
						const double t = (double)py[r * 64 + c] - m;
						v += t * t;
					}
					v /= 64.0;
					if (std::fabs(m) > pireMoy)
						pireMoy = std::fabs(m);
					if (std::fabs(v - attendu) > pireVar)
						pireVar = std::fabs(v - attendu);
				}
				const bool ok = pireMoy < 1e-6 && pireVar < 1e-6;
				(ok ? g_pass : g_fail)++;
				printf("  [ %s ] %-24s moyenne %.1e, variance vs s2/(s2+eps) %.1e\n", ok ? "OK" : "KO",
					   "LayerNorm intrinseque", pireMoy, pireVar);
			}

			{ // Gelu
				NkShape s;
				s.PushBack(32);
				s.PushBack(40);
				const NkTensor x = randTensor(s), w = randTensor(s);

				NkVar xc = NkVar::Leaf(x.Clone(), true);
				NkVar yc = autograd::Gelu(xc);
				autograd::Sum(autograd::Mul(yc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar xg = NkVar::Leaf(x.Clone().ToGPU(), true);
				NkVar yg = autograd::Gelu(xg);
				autograd::Sum(autograd::Mul(yg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("Gelu avant", yc.Value(), yg.Value(), kTol);
				compare("Gelu arriere", xc.Grad(), xg.Grad(), kTol);
			}

			{ // SoftmaxCausal : [.., T, T]
				const int64 T = 16;
				NkShape s;
				s.PushBack(3);
				s.PushBack(T);
				s.PushBack(T);
				const NkTensor x = randTensor(s), w = randTensor(s);

				NkVar xc = NkVar::Leaf(x.Clone(), true);
				NkVar yc = autograd::SoftmaxCausal(xc);
				autograd::Sum(autograd::Mul(yc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar xg = NkVar::Leaf(x.Clone().ToGPU(), true);
				NkVar yg = autograd::SoftmaxCausal(xg);
				autograd::Sum(autograd::Mul(yg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("SoftmaxCausal avant", yc.Value(), yg.Value(), kTol);
				compare("SoftmaxCausal arriere", xc.Grad(), xg.Grad(), kTol);

				// Propriété intrinsèque : chaque ligne somme à 1, et les
				// positions futures sont STRICTEMENT nulles. Un masque causal
				// décalé d'une case ne se verrait pas autrement — et il
				// laisserait le modèle lire le token suivant.
				NkTensor yy = yg.Value().ToCPU().Contiguous();
				const float *py = yy.DataAs<float>();
				double pireSomme = 0.0;
				bool futurPropre = true;
				for (int64 b = 0; b < 3; ++b)
					for (int64 q = 0; q < T; ++q) {
						double som = 0.0;
						for (int64 k = 0; k < T; ++k) {
							const double v = (double)py[(b * T + q) * T + k];
							som += v;
							if (k > q && v != 0.0)
								futurPropre = false;
						}
						if (std::fabs(som - 1.0) > pireSomme)
							pireSomme = std::fabs(som - 1.0);
					}
				const bool ok = pireSomme < 1e-5 && futurPropre;
				(ok ? g_pass : g_fail)++;
				printf("  [ %s ] %-24s somme-1 max %.1e, futur strictement nul = %s\n", ok ? "OK" : "KO",
					   "SoftmaxCausal intrins.", pireSomme, futurPropre ? "oui" : "NON");
			}

			{ // Embedding : table [V, d], indices [N]
				const int64 V = 50, dd = 32, N = 20;
				NkShape ts;
				ts.PushBack(V);
				ts.PushBack(dd);
				NkShape is;
				is.PushBack(N);
				const NkTensor tab = randTensor(ts);
				NkTensor idx = NkTensor::Zeros(is);
				// Les BORNES d'abord : un décalage d'un rang se manifeste là avant
				// partout ailleurs (0 lirait hors table par en dessous, V−1 par
				// au-dessus). Le reste balaie le vocabulaire.
				for (int64 i = 0; i < N; ++i)
					idx.DataAs<float>()[i] = (float)((i * 7 + 3) % V);
				idx.DataAs<float>()[0] = 0.0f;
				idx.DataAs<float>()[1] = (float)(V - 1);
				idx.DataAs<float>()[2] = 1.0f;
				idx.DataAs<float>()[3] = (float)(V - 2);
				NkShape os;
				os.PushBack(N);
				os.PushBack(dd);
				const NkTensor w = randTensor(os);

				NkVar tc = NkVar::Leaf(tab.Clone(), true);
				NkVar yc = autograd::Embedding(tc, idx);
				autograd::Sum(autograd::Mul(yc, NkVar::Leaf(w.Clone(), false))).Backward();

				NkVar tg = NkVar::Leaf(tab.Clone().ToGPU(), true);
				NkVar yg = autograd::Embedding(tg, idx);
				autograd::Sum(autograd::Mul(yg, NkVar::Leaf(w.Clone().ToGPU(), false))).Backward();

				compare("Embedding avant", yc.Value(), yg.Value(), kTol);
				compare("Embedding arriere", tc.Grad(), tg.Grad(), kTol);

				// ⚠️ EMBEDDING EST LE CAS OÙ L'ORACLE EST LE PLUS AVEUGLE.
				// Sans arithmétique, l'écart CPU/GPU est exactement nul — et il le
				// resterait si les DEUX chemins lisaient la ligne k+1 au lieu de k,
				// puisqu'ils partagent l'indexation. Le modèle apprendrait alors sur
				// des tokens décalés, sans rien qui plante, avec une perte seulement
				// un peu moins bonne : personne ne le remarquerait.
				//
				// La propriété définitionnelle se passe d'oracle : la ligne de sortie
				// i DOIT être la ligne idx[i] de la TABLE. On compare à la table,
				// jamais à l'autre chemin.
				NkTensor yy = yg.Value().ToCPU().Contiguous(), tt = tab.Contiguous();
				const float *py = yy.DataAs<float>();
				const float *pt = tt.DataAs<float>();
				const float *pi = idx.DataAs<float>();
				double pire = 0.0;
				for (int64 i = 0; i < N; ++i) {
					const int64 k = (int64)(pi[i] + 0.5f);
					for (int64 c = 0; c < dd; ++c) {
						const double e = std::fabs((double)py[i * dd + c] - (double)pt[k * dd + c]);
						if (e > pire)
							pire = e;
					}
				}
				// Une copie doit être EXACTE : aucune tolérance à accorder ici.
				const bool ok = (pire == 0.0);
				(ok ? g_pass : g_fail)++;
				printf("  [ %s ] %-24s ligne i == table[idx[i]] (bornes 0 et V-1 incluses), ecart %.1e\n",
					   ok ? "OK" : "KO", "Embedding definition", pire);
			}
		}

		// Le compteur de défauts GPU doit être resté à zéro. Il agrège désormais
		// les échecs de COMPILATION de noyau et les sorties entièrement nulles :
		// un test qui passerait avec un compteur non nul aurait comparé deux
		// résultats dont l'un n'a pas été calculé.
		const int64 defauts = NkTensorGpu::DefautCount();
		const bool zeroDefaut = (defauts == 0);
		(zeroDefaut ? g_pass : g_fail)++;
		printf("  [ %s ] %-24s %lld défaut(s) GPU signalé(s) pendant les tests\n", zeroDefaut ? "OK" : "KO",
			   "compteur de defauts", (long long)defauts);
	}

	// =========================================================================
	// TEST DE LA LIGNE ABSENTE — le weight tying crée-t-il bien DEUX chemins
	// de gradient vers la table d'embedding ?
	//
	// Sans tying, la table ne reçoit du gradient que par la RECHERCHE : creuse,
	// seules les lignes des tokens présents dans le lot. Avec tying, elle en
	// reçoit AUSSI par la PROJECTION DE SORTIE : dense, toutes les lignes à
	// chaque pas — y compris celles de tokens qui n'apparaissent nulle part.
	//
	// Ce test le rend CONSTATABLE au lieu de le supposer, sans lancer de course :
	// on prend des identifiants absents du lot et on regarde si leur ligne bouge.
	// C'est la mesure qui tranche entre « le tying dégrade par principe » et
	// « il change la nature du gradient reçu par la table ».
	{
		printf("\n--- Test de la ligne absente (deux chemins de gradient) ---\n");
		const int64 V = 20, D = 8, N = 4;
		NkShape ts;
		ts.PushBack(V);
		ts.PushBack(D);
		NkTensor tab0 = NkTensor::Zeros(ts);
		{
			uint64 r = 0x9E3779B97F4A7C15ull;
			float *p = tab0.DataAs<float>();
			for (int64 i = 0; i < V * D; ++i) {
				r = r * 6364136223846793005ull + 1442695040888963407ull;
				p[i] = (float)((double)((r >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52) * 0.04 - 0.02);
			}
		}
		// Le lot n'utilise QUE les identifiants 0..3 : les lignes 4..19 sont absentes.
		NkShape is;
		is.PushBack(N);
		NkTensor idx = NkTensor::Zeros(is);
		for (int64 i = 0; i < N; ++i)
			idx.DataAs<float>()[i] = (float)i;

		// Amplitude max du gradient sur les lignes ABSENTES (4..19).
		auto ampliAbsentes = [&](const NkTensor &g) {
			if (!g.IsValid())
				return -1.0; // pas de gradient du tout : cas distinct de « nul »
			NkTensor c = g.ToCPU().Contiguous();
			const float *p = c.DataAs<float>();
			double m = 0.0;
			for (int64 k = N; k < V; ++k)
				for (int64 j = 0; j < D; ++j) {
					const double a = std::fabs((double)p[k * D + j]);
					if (a > m)
						m = a;
				}
			return m;
		};

		// (a) Chemin SEUL de la recherche — ce que fait une tête LIBRE.
		NkVar tA = NkVar::Leaf(tab0.Clone(), true);
		autograd::Sum(autograd::Embedding(tA, idx)).Backward();
		const double aLibre = ampliAbsentes(tA.Grad());

		// (b) Recherche + projection liée — ce que fait le TYING.
		NkVar tB = NkVar::Leaf(tab0.Clone(), true);
		NkVar emb = autograd::Embedding(tB, idx); // [N, D]
		NkShape ordre;
		ordre.PushBack(1);
		ordre.PushBack(0);
		NkVar logits = autograd::Matmul(emb, autograd::Permute(tB, ordre)); // [N, V]
		autograd::Sum(autograd::Add(logits, autograd::Sum(emb))).Backward();
		const double aLie = ampliAbsentes(tB.Grad());

		printf("  lignes absentes du lot : identifiants %lld..%lld\n", (long long)N, (long long)(V - 1));
		printf("  gradient max sur ces lignes, recherche SEULE (tête libre) : %.3e\n", aLibre);
		printf("  gradient max sur ces lignes, recherche + projection LIÉE  : %.3e\n", aLie);

		// Attendu : nul côté tête libre, NON nul côté tying. C'est exactement la
		// différence de nature annoncée — et elle se constate sans entraînement.
		const bool libreNul = (aLibre == 0.0);
		const bool lieNonNul = (aLie > 0.0);
		const bool ok = libreNul && lieNonNul;
		(ok ? g_pass : g_fail)++;
		printf("  [ %s ] %-24s libre = 0 : %s | lié ≠ 0 : %s\n", ok ? "OK" : "KO", "mecanisme (synthetique)",
			   libreNul ? "oui" : "NON", lieNonNul ? "oui" : "NON");

		// ⚠️ CE QUI PRÉCÈDE NE SUFFIT PAS. Il éprouve le MÉCANISME, reconstruit à
		// partir des primitives — pas le modèle réellement entraîné. Un NkLlamaLM
		// dont la tête serait mal câblée passerait ce test sans problème, puisqu'il
		// n'y participe pas. On refait donc la mesure sur le VRAI modèle, avec un
		// lot ordinaire : c'est lui qui dit si les +0,0297 nat mesurés portent sur
		// un tying RÉEL ou seulement NOMINAL.
		{
			const uint32 Vm = 64, Dm = 32, Hm = 4, Lm = 2;
			const int64 B = 2, T = 8;
			NkShape tk;
			tk.PushBack(B);
			tk.PushBack(T);
			NkTensor tokens = NkTensor::Zeros(tk);
			// Le lot n'emploie QUE les identifiants 0..7. Les lignes 8..63 sont absentes.
			for (int64 i = 0; i < B * T; ++i)
				tokens.DataAs<float>()[i] = (float)(i % 8);

			auto gradAbsentes = [&](bool tied) {
				nn::NkLlamaLM m(Vm, Dm, Hm, Lm, 1234u, tied);
				NkVector<NkVar> ps;
				m.Parameters(ps);
				if (ps.Size() == 0)
					return -1.0;
				autograd::Sum(m.Forward(tokens)).Backward();
				const NkVar &emb = ps[0]; // mTokEmb est le PREMIER parametre pousse
				if (!emb.Grad().IsValid())
					return -1.0;
				NkTensor g = emb.Grad().ToCPU().Contiguous();
				const float *p = g.DataAs<float>();
				double mx = 0.0;
				for (int64 k = 8; k < (int64)Vm; ++k)
					for (int64 j = 0; j < (int64)Dm; ++j) {
						const double a = std::fabs((double)p[k * (int64)Dm + j]);
						if (a > mx)
							mx = a;
					}
				return mx;
			};

			const double gLibre = gradAbsentes(false);
			const double gLie = gradAbsentes(true);
			printf("  NkLlamaLM REEL, lot n'employant que les identifiants 0..7 :\n");
			printf("    gradient max sur les lignes 8..%u, tete LIBRE : %.3e\n", Vm - 1, gLibre);
			printf("    gradient max sur les lignes 8..%u, tete LIEE  : %.3e\n", Vm - 1, gLie);
			const bool okReel = (gLibre == 0.0) && (gLie > 0.0);
			(okReel ? g_pass : g_fail)++;
			printf("  [ %s ] %-24s tying REEL (chemin de sortie connecte) : %s\n", okReel ? "OK" : "KO",
				   "modele reel", okReel ? "oui" : "NON — le tying serait NOMINAL");
		}
	}

	// =========================================================================
	// MESURE ISOLÉE : le coût par FLOP dépend-il de la DIMENSION ?
	//
	// Constat qui l'a motivée (14 août 2026) : sur quatre configurations
	// d'entraînement, `temps/FLOPs` valait 85,4 et 85,6 à d=512, mais 35,6 à
	// d=640 — soit 2,4× moins cher par opération à la dimension NON puissance
	// de deux. Deux valeurs qui s'accordent à 0,2 % ne sortent pas d'une
	// contamination aléatoire ; et une contamination ne peut que RALENTIR, or
	// l'anomalie est une VITESSE.
	//
	// Hypothèse : 512 = 2^9. Sur GPU, une dimension principale en puissance de
	// deux fait tomber les accès dans les mêmes bancs de mémoire partagée et
	// alias le cache. L'effet est connu et spectaculaire sur les produits
	// matriciels ; la parade habituelle est de rembourrer la dimension.
	//
	// 528 et 576 départagent : s'ils sont rapides eux aussi, c'est bien la
	// puissance de deux qui est en cause, et non la valeur 640 en particulier.
	//
	// MÉTHODE : on retient le MINIMUM sur plusieurs répétitions. Une contention
	// extérieure ne peut qu'AJOUTER du temps — le minimum est donc l'estimateur
	// le moins pollué, et il ne demande pas une machine au repos.
	{
		printf("\n--- Coût par FLOP selon la dimension (produit matriciel isolé) ---\n");
		if (!NkTensorGpu::Get().IsAvailable()) {
			++g_fail;
			printf("  [ KO ] aucun device GPU : mesure impossible\n");
		} else {
			const int64 dims[5] = {512, 528, 576, 640, 768};
			const int64 M = 2048;
			const int kRep = 12;
			double refCout = 0.0;
			for (int di = 0; di < 5; ++di) {
				const int64 d = dims[di];
				NkShape as, bs;
				as.PushBack(M);
				as.PushBack(d);
				bs.PushBack(d);
				bs.PushBack(d);
				NkTensor A = NkTensor::Zeros(as).ToGPU();
				NkTensor B = NkTensor::Zeros(bs).ToGPU();
				double best = 1e30;
				for (int r = 0; r < kRep; ++r) {
					NkChrono c;
					NkTensor C = NkGpuMatmul(A, B);
					// Forcer l'achèvement : sans relecture, on chronomètre la
					// SOUMISSION du travail, pas son exécution.
					volatile float sentinelle = C.ToCPU().Contiguous().DataAs<float>()[0];
					(void)sentinelle;
					const double ms = c.Elapsed().ToMilliseconds();
					if (ms < best)
						best = ms;
				}
				// FLOPs = 2·M·d·d. On rapporte le coût par giga-FLOP.
				const double gflop = 2.0 * (double)M * (double)d * (double)d / 1e9;
				const double cout = best / gflop; // ms par GFLOP
				if (di == 0)
					refCout = cout;
				const bool p2 = ((d & (d - 1)) == 0);
				printf("  d=%-4lld %s  min sur %d = %7.2f ms   coût = %6.3f ms/GFLOP   (%.2fx la réf d=512)\n",
					   (long long)d, p2 ? "[2^n]" : "     ", kRep, best, cout, cout / refCout);
			}
			printf("  Si le coût par GFLOP est CONSTANT, la dimension n'y est pour rien.\n");
			printf("  S'il chute hors des puissances de deux, l'hypothèse est confirmée.\n");
			++g_pass; // mesure informative : elle rapporte, elle ne juge pas
		}
	}

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
