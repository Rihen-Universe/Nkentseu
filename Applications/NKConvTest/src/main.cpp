// =============================================================================
// NKConvTest — un vrai réseau CONVOLUTIONNEL classe des images (NKNN Jalon 2).
//   Architecture : Conv2D(1->4,3x3) -> ReLU -> MaxPool(2) -> Flatten -> Dense(->4)
//                  -> CrossEntropy, entraîné par Adam.
//   Jeu : 4 motifs 8x8 (barres, diagonale, cadre) + bruit. Doit converger ~100%.
//
// ⚠️ CE QUE CE TEST NE TESTE PAS : LA SÉMANTIQUE DU GRADIENT.
//
// Ses deux boucles sont sur Adam, dont le pas `m̂/(√v̂+ε)` est quasi INVARIANT à
// l'échelle du gradient. Mesuré le 2026-08-16 : quand `Backward()` a cessé de
// remettre les feuilles à zéro — et qu'il manquait donc les `ZeroGrad()`
// ci-dessous — ce test est resté à 2 OK / 0 échec, alors que les paramètres
// étaient entraînés sur une SOMME CUMULÉE des gradients depuis l'époque 0.
// `NKNNTest`, lui, est tombé au premier coup parce que son XOR est sur SGD.
//
// Autrement dit : ce fichier ne peut pas échouer sur un défaut d'accumulation ou
// de remise à zéro. Il vérifie que la convolution apprend, rien de plus. Le
// témoin qui discrimine vit dans `NKAutogradTest` (deux `Backward()` successifs
// sur la même feuille : `g2` doit valoir `2b`, pas `b`). Ne comptez pas sur le
// vert d'ici pour valider quoi que ce soit sur les gradients.
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKLogger/NkLog.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 WH = 8, D = WH * WH;

static void Prototype(uint32 kind, float *out) {
	for (uint32 i = 0; i < D; ++i)
		out[i] = 0.f;
	for (uint32 y = 0; y < WH; ++y)
		for (uint32 x = 0; x < WH; ++x) {
			bool on = false;
			switch (kind) {
				case 0:
					on = (x == 3 || x == 4);
					break; // barre verticale
				case 1:
					on = (y == 3 || y == 4);
					break; // barre horizontale
				case 2:
					on = (x == y || x + 1 == y || x == y + 1);
					break; // diagonale
				default:
					on = (x == 0 || y == 0 || x == WH - 1 || y == WH - 1); // cadre
			}
			if (on)
				out[y * WH + x] = 1.f;
		}
}

int main() {
	printf("=== NKConvTest : un CNN classe des images 8x8 ===\n\n");

	// Jeu : 4 classes x 12 = 48 images, forme [N,1,8,8].
	const uint32 NCLS = 4, PER = 12, N = NCLS * PER;
	NkTensor X = NkTensor::Zeros(NkShape{(int64)N, 1, (int64)WH, (int64)WH});
	NkVector<int32> labels;
	{
		float *xp = X.DataAs<float>();
		float proto[D];
		uint32 s = 1234u;
		for (uint32 c = 0; c < NCLS; ++c) {
			Prototype(c, proto);
			for (uint32 k = 0; k < PER; ++k) {
				uint32 i = c * PER + k;
				for (uint32 j = 0; j < D; ++j) {
					s = s * 1664525u + 1013904223u;
					float noise = ((float)((s >> 9) & 0x7FFFu) / 32767.f - 0.5f) * 0.3f;
					float v = proto[j] + noise;
					xp[i * D + j] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				}
				labels.PushBack((int32)c);
			}
		}
	}
	NkTensor Oh = nn::OneHot(labels.Data(), N, NCLS);

	// CNN : Conv(1->4, 3x3, pad 1) -> ReLU -> MaxPool(2) -> Flatten(4*4*4=64) -> Dense(64->4).
	nn::NkConv2D conv(1, 4, 3, /*stride*/ 1, /*pad*/ 1, 111u);
	nn::NkDense fc(4 * 4 * 4, NCLS, 222u);

	NkVector<NkVar> params;
	conv.Parameters(params);
	fc.Parameters(params);
	optim::NkAdam adam(params, 0.01f);

	NkVar xin = NkVar::Leaf(X, false);
	NkVar yoh = NkVar::Leaf(Oh, false);

	auto forward = [&](const NkVar &x) {
		NkVar h = nn::Relu(conv.Forward(x)); // [N,4,8,8]
		h = nn::MaxPool2D(h, 2, 2);			 // [N,4,4,4]
		h = nn::Flatten(h);					 // [N,64]
		return fc.Forward(h);				 // [N,4] logits
	};

	printf("-- Entraînement (Adam + CrossEntropy) --\n");
	double loss = 0.0;
	for (int e = 0; e <= 300; ++e) {
		adam.ZeroGrad(); // Backward() n'efface plus les feuilles : un pas = un gradient
		NkVar logits = forward(xin);
		NkVar L = nn::CrossEntropyLoss(logits, yoh);
		L.Backward();
		adam.Step();
		loss = L.Value().GetItem(NkShape{(int64)0});
		if (e % 50 == 0)
			printf("  époque %3d : perte CE = %.5f\n", e, loss);
	}

	// Exactitude (argmax des logits).
	NkTensor logits = forward(xin).Value().Contiguous();
	const float *lp = logits.DataAs<float>();
	int good = 0;
	for (uint32 i = 0; i < N; ++i) {
		int best = 0;
		float bv = lp[i * NCLS];
		for (uint32 c = 1; c < NCLS; ++c)
			if (lp[i * NCLS + c] > bv) {
				bv = lp[i * NCLS + c];
				best = (int)c;
			}
		if (best == labels[i])
			++good;
	}
	const double acc = (double)good / (double)N;
	const bool ok = acc >= 0.95;
	printf("\n  [ %s ] CNN : %d/%d = %.1f%% exactitude (perte finale %.5f)\n", ok ? "OK" : "KO", good, (int)N,
		   acc * 100.0, loss);

	// =========================================================================
	// NKNN : NkCNN prêt à l'emploi (NkSequential) — MÊME jeu de données (X/labels/
	// Oh), architecture équivalente (Conv(1->4,3x3,pad1)->ReLU->MaxPool->Flatten->
	// Dense) mais assemblée par la classe générique au lieu d'un forward écrit à la
	// main. Résultat réel de convergence, indépendant du modèle manuel ci-dessus.
	// =========================================================================
	logger.Info("-- NKNN : NkCNN pret a l'emploi (NkSequential) --");
	bool cnnOk = false;
	{
		nn::NkCNN cnn(NkVector<uint32>{1, 4}, /*kernel*/ 3, /*inputHW*/ WH, /*numClasses*/ NCLS, /*pool*/ true,
					  555u);
		NkVector<NkVar> cnnParams;
		cnn.Parameters(cnnParams);
		optim::NkAdam cnnAdam(cnnParams, 0.01f);

		double cnnLoss = 0.0;
		for (int e = 0; e <= 300; ++e) {
			cnnAdam.ZeroGrad(); // un pas = un gradient
			NkVar logitsCnn = cnn.Forward(xin);
			NkVar lossCnn = nn::CrossEntropyLoss(logitsCnn, yoh);
			lossCnn.Backward();
			cnnAdam.Step();
			cnnLoss = lossCnn.Value().GetItem(NkShape{(int64)0});
		}

		NkTensor logitsF = cnn.Forward(xin).Value().Contiguous();
		const float *flp = logitsF.DataAs<float>();
		int cnnGood = 0;
		for (uint32 i = 0; i < N; ++i) {
			int best = 0;
			float bv = flp[i * NCLS];
			for (uint32 c = 1; c < NCLS; ++c)
				if (flp[i * NCLS + c] > bv) {
					bv = flp[i * NCLS + c];
					best = (int)c;
				}
			if (best == labels[i])
				++cnnGood;
		}
		const double cnnAcc = (double)cnnGood / (double)N;
		// 2 briques a parametres (Conv2D + Dense) -> 4 tenseurs (W+b chacune).
		cnnOk = (cnnAcc >= 0.95) && (cnnParams.Size() == 4);
		logger.Info("  [ {0} ] NkCNN({1} parametres) : {2}/{3} = {4:.4}% exactitude (perte finale {5:.6})",
					cnnOk ? "OK" : "KO", cnnParams.Size(), cnnGood, (int)N, cnnAcc * 100.0, cnnLoss);
	}

	const int totalOk = (ok ? 1 : 0) + (cnnOk ? 1 : 0);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", totalOk, 2 - totalOk);
	return (totalOk == 2) ? 0 : 1;
}
