// =============================================================================
// NKRnnCtcTest — preuve des cellules récurrentes (GRU/LSTM) et de la perte CTC.
//
//   1) Vérifie les gradients des cellules GRU et LSTM (p/r à l'entrée) contre des
//      différences finies centrées.
//   2) Entraîne un GRU + tête linéaire sur une séquence jouet avec la perte CTC
//      (sans alignement) : la perte doit chuter et le décodage GLOUTON (argmax +
//      collapse + retrait des blancs) doit retrouver la cible {0,1,2}.
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

static NkTensor Mat(const NkShape &shape, const float *data) {
	return NkTensor::FromData(shape, data, NkDType::NK_F32);
}

// dL/dx analytique (autograd) vs numérique (différences finies centrées).
template <typename F> static void GradCheck(const char *name, const NkTensor &x0, F buildLoss) {
	NkVar x = NkVar::Leaf(x0.Clone(), true);
	NkVar loss = buildLoss(x);
	loss.Backward();
	NkTensor ga = x.Grad().Contiguous();
	const float *gp = ga.DataAs<float>();

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
	printf("  [ %s ] %-16s  err max vs diff. finies = %.2e\n", ok ? "OK" : "KO", name, maxErr);
}

// Décodage CTC glouton : argmax par pas -> collapse répétitions -> retire blancs.
static void GreedyDecode(const NkTensor &logits /*[T,B,V]*/, int32 b, int32 blank, NkVector<int32> &out) {
	NkTensor lc = logits.Contiguous();
	const NkShape &sh = lc.Shape();
	const int64 T = sh[0], B = sh[1], V = sh[2];
	const float *lp = lc.DataAs<float>();
	int32 prev = -1;
	for (int64 t = 0; t < T; ++t) {
		const float *row = lp + (t * B + b) * V;
		int32 arg = 0;
		for (int64 k = 1; k < V; ++k)
			if (row[k] > row[arg])
				arg = (int32)k;
		if (arg != prev && arg != blank)
			out.PushBack(arg);
		prev = arg;
	}
}

int main() {
	printf("=== NKRnnCtcTest : GRU/LSTM (gradients) + CTC (entraînement) ===\n\n");

	// -----------------------------------------------------------------------
	// 1) Gradient-check des cellules récurrentes (p/r à l'entrée x, un pas).
	// -----------------------------------------------------------------------
	printf("-- Gradients des cellules (diff. finies) --\n");
	{
		nn::NkGRUCell gru(3, 4, 42u);
		float hd[8] = {0.1f, -0.2f, 0.3f, 0.0f, 0.2f, 0.1f, -0.1f, 0.4f}; // h [2,4]
		float xd[6] = {0.5f, -1.f, 0.3f, 0.2f, 0.7f, -0.4f};			 // x [2,3]
		NkTensor H = Mat(NkShape{2, 4}, hd);
		GradCheck("GRU cell dX", Mat(NkShape{2, 3}, xd), [&gru, H](NkVar x) {
					  NkVar h = gru.Forward(x, NkVar::Leaf(H, false));
					  return autograd::Sum(autograd::Mul(h, h)); // Σ h² : gradient non trivial
				  });
	}
	{
		nn::NkLSTMCell lstm(3, 4, 7u);
		float hd[8] = {0.1f, -0.2f, 0.3f, 0.0f, 0.2f, 0.1f, -0.1f, 0.4f};
		float cd[8] = {0.0f, 0.1f, -0.1f, 0.2f, 0.3f, -0.2f, 0.1f, 0.0f};
		float xd[6] = {0.5f, -1.f, 0.3f, 0.2f, 0.7f, -0.4f};
		NkTensor H = Mat(NkShape{2, 4}, hd), C = Mat(NkShape{2, 4}, cd);
		GradCheck("LSTM cell dX", Mat(NkShape{2, 3}, xd), [&lstm, H, C](NkVar x) {
					  nn::NkLSTMState s;
					  s.h = NkVar::Leaf(H, false);
					  s.c = NkVar::Leaf(C, false);
					  nn::NkLSTMState o = lstm.Forward(x, s);
					  return autograd::Sum(autograd::Mul(o.h, o.h));
				  });
	}

	// -----------------------------------------------------------------------
	// 2) Entraînement GRU + tête linéaire avec CTC sur une séquence jouet.
	//    V=4 (symboles 0,1,2 + blanc=3), T=10, batch=1, cible = {0,1,2}.
	// -----------------------------------------------------------------------
	printf("\n-- Entraînement CTC (GRU 4->24 + Dense 24->4, Adam) --\n");
	const int32 IN = 4, HID = 24, V = 4, BLANK = 3, T = 10;

	// Séquence d'entrée déterministe : à chaque pas, un vecteur distinct (signal
	// temporel exploitable par le GRU). xs[t] = [sin, cos, t/T, 1].
	NkVector<NkVar> xs;
	for (int32 t = 0; t < T; ++t) {
		float v[4] = {(float)std::sin(0.7 * t), (float)std::cos(0.7 * t), (float)t / (float)T, 1.0f};
		xs.PushBack(NkVar::Leaf(Mat(NkShape{1, IN}, v), false));
	}

	nn::NkGRUCell gru(IN, HID, 123u);
	nn::NkDense head(HID, V, 456u);

	NkVector<NkVar> params;
	gru.Parameters(params);
	head.Parameters(params);
	optim::NkAdam adam(params, 0.02f);

	// Cible CTC = {0,1,2}.
	NkVector<NkVector<int32>> targets;
	targets.Resize(1);
	targets[0].PushBack(0);
	targets[0].PushBack(1);
	targets[0].PushBack(2);

	double firstLoss = 0.0, lastLoss = 0.0;
	for (int step = 1; step <= 300; ++step) {
		NkVar h0 = nn::ZeroState(1, HID);
		NkVector<NkVar> hs = nn::GRURunSeq(gru, xs, h0);
		// Tête linéaire par pas -> logits, puis empilement [T,1,V].
		NkVector<NkVar> logitSteps;
		for (uint32 t = 0; t < hs.Size(); ++t)
			logitSteps.PushBack(head.Forward(hs[t]));
		NkVar logits = nn::StackTime(logitSteps); // [T,1,V]
		NkVar loss = autograd::CTCLoss(logits, targets, BLANK);

		adam.ZeroGrad();
		loss.Backward();
		adam.Step();

		lastLoss = loss.Value().GetItem(NkShape{(int64)0});
		if (step == 1)
			firstLoss = lastLoss;
		if (step % 50 == 0 || step == 1)
			printf("  step %3d : perte CTC = %.5f\n", step, lastLoss);
	}

	// Décodage glouton final.
	NkVar h0 = nn::ZeroState(1, HID);
	NkVector<NkVar> hs = nn::GRURunSeq(gru, xs, h0);
	NkVector<NkVar> logitSteps;
	for (uint32 t = 0; t < hs.Size(); ++t)
		logitSteps.PushBack(head.Forward(hs[t]));
	NkVar logits = nn::StackTime(logitSteps);
	NkVector<int32> decoded;
	GreedyDecode(logits.Value(), 0, BLANK, decoded);

	printf("  décodage glouton : {");
	for (uint32 i = 0; i < decoded.Size(); ++i)
		printf("%s%d", i ? "," : "", decoded[i]);
	printf("}  (cible {0,1,2})\n");

	const bool lossDropped = lastLoss < firstLoss * 0.2 && lastLoss < 0.5;
	bool decodeOk = decoded.Size() == 3 && decoded[0] == 0 && decoded[1] == 1 && decoded[2] == 2;
	(lossDropped ? g_pass : g_fail)++;
	(decodeOk ? g_pass : g_fail)++;
	printf("  [ %s ] la perte CTC a fortement chuté (%.4f -> %.4f)\n", lossDropped ? "OK" : "KO", firstLoss,
		   lastLoss);
	printf("  [ %s ] décodage glouton = cible\n", decodeOk ? "OK" : "KO");

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
