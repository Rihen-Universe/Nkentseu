// =============================================================================
// NKNNTest — entraîne XOR via l'API propre NKNN (Dense) + NKOptim (SGD).
//
// Même réseau que NKAutogradTest, mais assemblé avec des COUCHES réutilisables et
// un OPTIMISEUR (au lieu du SGD manuel). Prouve la pile Phase 2 :
//   NKTensor -> NKAutograd -> NKNN + NKOptim.
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
    printf("=== NKNNTest : XOR via NKNN (Dense) + NKOptim (SGD) ===\n\n");

    // Jeu XOR : 4 exemples.
    const float Xd[8] = { 0,0,  0,1,  1,0,  1,1 };
    const float Yd[4] = { 0,    1,    1,    0    };
    NkTensor X = NkTensor::FromData(NkShape{ 4, 2 }, Xd, NkDType::NK_F32);
    NkTensor Y = NkTensor::FromData(NkShape{ 4, 1 }, Yd, NkDType::NK_F32);

    // Modèle : Dense(2->8) -> tanh -> Dense(8->1) -> sigmoid.
    nn::NkDense l1(2, 8, 12345u);
    nn::NkDense l2(8, 1, 6789u);

    // Optimiseur : SGD sur tous les paramètres (W1,b1,W2,b2), momentum 0.9.
    NkVector<NkVar> params;
    l1.Parameters(params);
    l2.Parameters(params);
    optim::NkSGD opt(params, /*lr*/ 0.5f, /*momentum*/ 0.9f);

    NkVar xin = NkVar::Leaf(X, false);
    NkVar yt  = NkVar::Leaf(Y, false);

    auto forward = [&](const NkVar& x) {
        NkVar h = nn::Tanh(l1.Forward(x));
        return nn::Sigmoid(l2.Forward(h));
    };

    const int epochs = 5000;
    double lastLoss = 0.0;
    for (int e = 0; e <= epochs; ++e) {
        NkVar o    = forward(xin);
        NkVar loss = nn::MSELoss(o, yt);
        loss.Backward();
        opt.Step();
        lastLoss = loss.Value().GetItem(NkShape{ (int64)0 });
        if (e % 500 == 0)
            printf("  epoch %5d : perte = %.6f\n", e, lastLoss);
    }

    // Prédictions finales.
    NkVar o = forward(xin);
    const NkTensor& pred = o.Value();
    int correct = 0;
    printf("\n  XOR : entrée -> prédiction (cible)\n");
    for (int i = 0; i < 4; ++i) {
        double p = pred.GetItem(NkShape{ (int64)i, (int64)0 });
        bool   ok = (p > 0.5) == (Yd[i] > 0.5);
        correct += ok ? 1 : 0;
        printf("    [%d,%d] -> %.3f  (%.0f)  %s\n",
               (int)Xd[i * 2], (int)Xd[i * 2 + 1], p, (double)Yd[i], ok ? "OK" : "KO");
    }

    const bool xorOk = (correct == 4) && (lastLoss < 1e-2);
    printf("\n  [ %s ] XOR appris via NKNN+NKOptim (%d/4, perte finale %.6f)\n",
           xorOk ? "OK" : "KO", correct, lastLoss);

    // -----------------------------------------------------------------------
    // Classification 3 classes (Adam + entropie croisée) : 3 clusters 2D.
    // Modèle : Dense(2->16) -> relu -> Dense(16->3) -> logits ; CrossEntropyLoss.
    // -----------------------------------------------------------------------
    printf("\n-- Classification 3 classes (Adam + CrossEntropy) --\n");

    const uint32 NC = 3, PER = 12, NP = NC * PER;   // 36 points
    const float centers[3][2] = { { -2.f, -2.f }, { 2.f, -2.f }, { 0.f, 2.f } };
    NkTensor Xc = NkTensor::Zeros(NkShape{ (int64)NP, 2 });
    NkVector<int32> labels;
    {
        float* xp = Xc.DataAs<float>();
        uint32 s = 777u;
        auto jit = [&s]() { s = s * 1664525u + 1013904223u;
                            return ((float)((s >> 9) & 0x7FFFu) / 32767.0f - 0.5f) * 0.9f; };
        for (uint32 c = 0; c < NC; ++c)
            for (uint32 k = 0; k < PER; ++k) {
                uint32 i = c * PER + k;
                xp[i * 2 + 0] = centers[c][0] + jit();
                xp[i * 2 + 1] = centers[c][1] + jit();
                labels.PushBack((int32)c);
            }
    }
    NkTensor Oh = nn::OneHot(labels.Data(), NP, NC);

    nn::NkDense c1(2, 16, 4242u);
    nn::NkDense c2(16, NC, 909u);
    NkVector<NkVar> cparams;
    c1.Parameters(cparams);
    c2.Parameters(cparams);
    optim::NkAdam adam(cparams, /*lr*/ 0.02f);

    NkVar xin2 = NkVar::Leaf(Xc, false);
    NkVar yoh  = NkVar::Leaf(Oh, false);
    auto fwd2  = [&](const NkVar& x) { return c2.Forward(nn::Relu(c1.Forward(x))); };

    double clsLoss = 0.0;
    for (int e = 0; e <= 2000; ++e) {
        NkVar logits = fwd2(xin2);
        NkVar loss   = nn::CrossEntropyLoss(logits, yoh);
        loss.Backward();
        adam.Step();
        clsLoss = loss.Value().GetItem(NkShape{ (int64)0 });
        if (e % 400 == 0) printf("  epoch %5d : perte CE = %.6f\n", e, clsLoss);
    }

    // Exactitude finale (argmax des logits).
    NkTensor logits = fwd2(xin2).Value().Contiguous();
    const float* lp = logits.DataAs<float>();
    int good = 0;
    for (uint32 i = 0; i < NP; ++i) {
        int best = 0; float bv = lp[i * NC];
        for (uint32 c = 1; c < NC; ++c) if (lp[i * NC + c] > bv) { bv = lp[i * NC + c]; best = (int)c; }
        if (best == labels[i]) ++good;
    }
    const double acc = (double)good / (double)NP;
    const bool clsOk = acc >= 0.95;
    printf("  [ %s ] classification (%d/%d = %.1f%% exactitude, perte CE %.6f)\n",
           clsOk ? "OK" : "KO", good, (int)NP, acc * 100.0, clsLoss);

    const int pass = (xorOk ? 1 : 0) + (clsOk ? 1 : 0);
    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, 2 - pass);
    return (pass == 2) ? 0 : 1;
}
