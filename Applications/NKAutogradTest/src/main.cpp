// =============================================================================
// NKAutogradTest — preuve du moteur d'autodiff (NKAI, Phase 2).
//
//   1) Vérifie les gradients de chaque op (Mul, Matmul, Tanh, Sigmoid, ReLU, MSE)
//      contre des DIFFÉRENCES FINIES centrées (dL/dx ≈ (L(x+ε)-L(x-ε))/2ε).
//   2) Entraîne un MLP 2-8-1 sur XOR (tanh caché + sigmoid sortie + MSE, SGD
//      manuel) — construit UNIQUEMENT avec l'autograd. La perte doit chuter et
//      les 4 prédictions doivent être correctes.
// =============================================================================
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

// dL/dx analytique (autograd) vs numérique (différences finies centrées).
template <typename F>
static void GradCheck(const char* name, const NkTensor& x0, F buildLoss) {
    // Gradient analytique.
    NkVar x    = NkVar::Leaf(x0.Clone(), true);
    NkVar loss = buildLoss(x);
    loss.Backward();
    NkTensor    ga = x.Grad().Contiguous();
    const float* gp = ga.DataAs<float>();

    // Gradient numérique élément par élément.
    NkTensor xc = x0.Contiguous().Clone();
    float*   xp = xc.DataAs<float>();
    const int64 n   = NkShapeNumel(xc.Shape());
    const float eps = 1e-3f;
    double maxErr = 0.0;
    for (int64 i = 0; i < n; ++i) {
        const float o = xp[i];
        xp[i] = o + eps;
        double Lp = buildLoss(NkVar::Leaf(xc.Clone(), false)).Value().GetItem(NkShape{ (int64)0 });
        xp[i] = o - eps;
        double Lm = buildLoss(NkVar::Leaf(xc.Clone(), false)).Value().GetItem(NkShape{ (int64)0 });
        xp[i] = o;
        const double num = (Lp - Lm) / (2.0 * (double)eps);
        const double err = std::fabs(num - (double)gp[i]);
        if (err > maxErr) maxErr = err;
    }
    const bool ok = maxErr < 1e-2;
    (ok ? g_pass : g_fail)++;
    printf("  [ %s ] %-18s  err max vs diff. finies = %.2e\n", ok ? "OK" : "KO", name, maxErr);
}

static NkTensor Mat(const NkShape& shape, const float* data) {
    return NkTensor::FromData(shape, data, NkDType::NK_F32);
}

int main() {
    printf("=== NKAutogradTest : gradients (diff. finies) + XOR ===\n\n");

    printf("-- Vérification des gradients --\n");

    // 1) Sum(x ⊙ b) : dL/dx = b.
    {
        float bd[4] = { 0.5f, -1.0f, 2.0f, 0.25f };
        float xd[4] = { 1.0f,  2.0f, -1.0f, 3.0f };
        NkTensor b = Mat(NkShape{ 4 }, bd);
        GradCheck("Mul+Sum", Mat(NkShape{ 4 }, xd),
                  [b](NkVar x) { return autograd::Sum(autograd::Mul(x, NkVar::Leaf(b, false))); });
    }
    // 2) Sum(tanh(x)).
    {
        float xd[5] = { -1.5f, -0.3f, 0.0f, 0.7f, 2.0f };
        GradCheck("Tanh+Sum", Mat(NkShape{ 5 }, xd),
                  [](NkVar x) { return autograd::Sum(autograd::Tanh(x)); });
    }
    // 3) Sum(sigmoid(x)).
    {
        float xd[5] = { -2.0f, -0.5f, 0.1f, 1.0f, 3.0f };
        GradCheck("Sigmoid+Sum", Mat(NkShape{ 5 }, xd),
                  [](NkVar x) { return autograd::Sum(autograd::Sigmoid(x)); });
    }
    // 4) Sum(relu(x)) — x loin de 0 (dérivée non définie en 0).
    {
        float xd[5] = { -2.0f, -0.4f, 0.6f, 1.3f, 2.5f };
        GradCheck("Relu+Sum", Mat(NkShape{ 5 }, xd),
                  [](NkVar x) { return autograd::Sum(autograd::Relu(x)); });
    }
    // 5) Sum(A · B) : gradient par rapport à A.
    {
        float ad[6] = { 1, 2, 3, 4, 5, 6 };            // [2,3]
        float bd[6] = { 0.5f, 1, -1, 2, 0.25f, -0.5f };// [3,2]
        NkTensor B = Mat(NkShape{ 3, 2 }, bd);
        GradCheck("Matmul+Sum", Mat(NkShape{ 2, 3 }, ad),
                  [B](NkVar A) { return autograd::Sum(autograd::Matmul(A, NkVar::Leaf(B, false))); });
    }
    // 6) MSE(pred, cible) : dL/dpred = 2(pred-cible)/N.
    {
        float pd[4] = { 0.2f, 0.9f, 0.7f, 0.1f };
        float td[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
        NkTensor t = Mat(NkShape{ 4 }, td);
        GradCheck("MSE", Mat(NkShape{ 4 }, pd),
                  [t](NkVar p) { return autograd::MSE(p, NkVar::Leaf(t, false)); });
    }
    // 7) SoftmaxCrossEntropy(logits[B,C], onehot) : dLogits = (softmax-onehot)/B.
    {
        float ld[6] = { 2.0f, 0.5f, -1.0f,   0.1f, 1.5f, 0.3f };  // [2,3] logits
        float oh[6] = { 1, 0, 0,   0, 1, 0 };                     // classes 0 et 1
        NkTensor t = Mat(NkShape{ 2, 3 }, oh);
        GradCheck("SoftmaxCE", Mat(NkShape{ 2, 3 }, ld),
                  [t](NkVar z) { return autograd::SoftmaxCrossEntropy(z, NkVar::Leaf(t, false)); });
    }
    // 8) Conv2D — gradient p/r à l'ENTRÉE : input [1,1,4,4] ⊛ weight [1,1,2,2].
    {
        float xd[16] = { 1,2,0,1,  0,1,3,2,  2,0,1,1,  1,1,0,2 };
        float wd[4]  = { 1, -1, 0.5f, 2 };
        NkTensor Wt = Mat(NkShape{ 1,1,2,2 }, wd);
        GradCheck("Conv2D dX", Mat(NkShape{ 1,1,4,4 }, xd),
                  [Wt](NkVar x) { return autograd::Sum(autograd::Conv2D(x, NkVar::Leaf(Wt, false), 1, 0)); });
    }
    // 9) Conv2D — gradient p/r aux POIDS.
    {
        float xd[16] = { 1,2,0,1,  0,1,3,2,  2,0,1,1,  1,1,0,2 };
        float wd[4]  = { 1, -1, 0.5f, 2 };
        NkTensor Xc = Mat(NkShape{ 1,1,4,4 }, xd);
        GradCheck("Conv2D dW", Mat(NkShape{ 1,1,2,2 }, wd),
                  [Xc](NkVar w) { return autograd::Sum(autograd::Conv2D(NkVar::Leaf(Xc, false), w, 1, 0)); });
    }
    // 10) MaxPool2D — gradient routé vers l'argmax (maxima UNIQUES par fenêtre :
    //     les diff. finies sont invalides sur une égalité, l'argmax basculant).
    {
        float xd[16] = { 1,3,2,9,  4,1,7,2,  0,5,1,1,  2,0,3,6 };
        GradCheck("MaxPool2D", Mat(NkShape{ 1,1,4,4 }, xd),
                  [](NkVar x) { return autograd::Sum(autograd::MaxPool2D(x, 2, 2)); });
    }
    // 11) ConvTranspose2D — gradient p/r à l'ENTRÉE : input [1,1,2,2], weight [1,1,2,2].
    {
        float xd[4] = { 1, 2, 3, 4 };
        float wd[4] = { 1, -1, 0.5f, 2 };
        NkTensor Wt = Mat(NkShape{ 1,1,2,2 }, wd);
        GradCheck("ConvT2D dX", Mat(NkShape{ 1,1,2,2 }, xd),
                  [Wt](NkVar x) { return autograd::Sum(autograd::ConvTranspose2D(x, NkVar::Leaf(Wt, false), 1, 0)); });
    }
    // 12) ConvTranspose2D — gradient p/r aux POIDS.
    {
        float xd[4] = { 1, 2, 3, 4 };
        float wd[4] = { 1, -1, 0.5f, 2 };
        NkTensor Xc = Mat(NkShape{ 1,1,2,2 }, xd);
        GradCheck("ConvT2D dW", Mat(NkShape{ 1,1,2,2 }, wd),
                  [Xc](NkVar w) { return autograd::Sum(autograd::ConvTranspose2D(NkVar::Leaf(Xc, false), w, 1, 0)); });
    }
    // 13) Exp — d/dx exp(x) = exp(x).
    {
        float xd[4] = { -1.f, 0.f, 0.5f, 1.2f };
        GradCheck("Exp", Mat(NkShape{ 4 }, xd),
                  [](NkVar x) { return autograd::Sum(autograd::Exp(x)); });
    }
    // 14-15) Conv3D — gradients p/r entrée et poids : [1,1,3,3,3] ⊛ [1,1,2,2,2].
    {
        float xd[27] = { 1,2,0, 1,0,3, 2,1,0,  0,1,2, 3,0,1, 1,2,0,  2,0,1, 0,3,2, 1,0,1 };
        float wd[8]  = { 1,-1,0.5f,2,  -0.5f,1,0,1.5f };
        NkTensor Wt = Mat(NkShape{ 1,1,2,2,2 }, wd);
        GradCheck("Conv3D dX", Mat(NkShape{ 1,1,3,3,3 }, xd),
                  [Wt](NkVar x) { return autograd::Sum(autograd::Conv3D(x, NkVar::Leaf(Wt, false), 1, 0)); });
        NkTensor Xc = Mat(NkShape{ 1,1,3,3,3 }, xd);
        GradCheck("Conv3D dW", Mat(NkShape{ 1,1,2,2,2 }, wd),
                  [Xc](NkVar w) { return autograd::Sum(autograd::Conv3D(NkVar::Leaf(Xc, false), w, 1, 0)); });
    }
    // 16-17) ConvTranspose3D — gradients : [1,1,2,2,2] -> upsample.
    {
        float xd[8] = { 1,2,3,4, 0,1,2,1 };
        float wd[8] = { 1,-1,0.5f,2,  -0.5f,1,0,1.5f };
        NkTensor Wt = Mat(NkShape{ 1,1,2,2,2 }, wd);
        GradCheck("ConvT3D dX", Mat(NkShape{ 1,1,2,2,2 }, xd),
                  [Wt](NkVar x) { return autograd::Sum(autograd::ConvTranspose3D(x, NkVar::Leaf(Wt, false), 1, 0)); });
        NkTensor Xc = Mat(NkShape{ 1,1,2,2,2 }, xd);
        GradCheck("ConvT3D dW", Mat(NkShape{ 1,1,2,2,2 }, wd),
                  [Xc](NkVar w) { return autograd::Sum(autograd::ConvTranspose3D(NkVar::Leaf(Xc, false), w, 1, 0)); });
    }
    // 18) SigmoidBCE — dLogits = (sigmoid(logits) - cible)/N.
    {
        float ld[6] = { -1.5f, 0.3f, 2.0f, -0.7f, 1.1f, 0.0f };
        float td[6] = { 0.f, 1.f, 1.f, 0.f, 1.f, 0.f };
        NkTensor t = Mat(NkShape{ 6 }, td);
        GradCheck("SigmoidBCE", Mat(NkShape{ 6 }, ld),
                  [t](NkVar z) { return autograd::SigmoidBCE(z, NkVar::Leaf(t, false)); });
    }
    // 19) Upsample2x — chaque entrée répétée 4× (gradient = 4).
    {
        float xd[4] = { 1, 2, 3, 4 };
        GradCheck("Upsample2x", Mat(NkShape{ 1,1,2,2 }, xd),
                  [](NkVar x) { return autograd::Sum(autograd::Upsample2x(x)); });
    }

    // -----------------------------------------------------------------------
    // Entraînement XOR : MLP 2-8-1, tanh caché + sigmoid sortie + MSE, SGD.
    // -----------------------------------------------------------------------
    printf("\n-- Entraînement XOR (MLP 2-8-1, SGD via autograd) --\n");

    const float Xd[8] = { 0,0,  0,1,  1,0,  1,1 };
    const float Yd[4] = { 0,    1,    1,    0    };
    NkTensor X = Mat(NkShape{ 4, 2 }, Xd);
    NkTensor Y = Mat(NkShape{ 4, 1 }, Yd);

    const int H = 8;
    NkTensor W1 = NkTensor::Zeros(NkShape{ 2, H });
    NkTensor b1 = NkTensor::Zeros(NkShape{ 1, H });
    NkTensor W2 = NkTensor::Zeros(NkShape{ H, 1 });
    NkTensor b2 = NkTensor::Zeros(NkShape{ 1, 1 });

    // Init pseudo-aléatoire (LCG déterministe) dans [-0.5, 0.5] pour briser la symétrie.
    uint32 seed = 12345u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return ((float)((seed >> 8) & 0xFFFFu) / 65535.0f - 0.5f);
    };
    { float* p = W1.DataAs<float>(); for (int64 i = 0; i < NkShapeNumel(W1.Shape()); ++i) p[i] = rnd(); }
    { float* p = W2.DataAs<float>(); for (int64 i = 0; i < NkShapeNumel(W2.Shape()); ++i) p[i] = rnd(); }

    const float lr     = 1.0f;
    const int   epochs = 8000;
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
            printf("  epoch %5d : perte = %.6f\n", e, loss.Value().GetItem(NkShape{ (int64)0 }));
    }

    // Prédiction finale.
    NkVar h = autograd::Tanh(autograd::Add(autograd::Matmul(NkVar::Leaf(X, false), NkVar::Leaf(W1, false)),
                                           NkVar::Leaf(b1, false)));
    NkVar o = autograd::Sigmoid(autograd::Add(autograd::Matmul(h, NkVar::Leaf(W2, false)),
                                              NkVar::Leaf(b2, false)));
    const NkTensor& pred = o.Value();
    int correct = 0;
    printf("\n  XOR : entrée -> prédiction (cible)\n");
    for (int i = 0; i < 4; ++i) {
        double p = pred.GetItem(NkShape{ (int64)i, (int64)0 });
        double target = Yd[i];
        bool   ok = (p > 0.5) == (target > 0.5);
        correct += ok ? 1 : 0;
        printf("    [%d,%d] -> %.3f  (%.0f)  %s\n",
               (int)Xd[i * 2], (int)Xd[i * 2 + 1], p, target, ok ? "OK" : "KO");
    }
    const bool xorOk = (correct == 4);
    (xorOk ? g_pass : g_fail)++;
    printf("  [ %s ] XOR appris (%d/4 prédictions correctes)\n", xorOk ? "OK" : "KO", correct);

    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
