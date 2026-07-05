// =============================================================================
// NKInferTest — persistance + inférence (NKInfer).
//   1) Entraîne un modèle (Adam+CE sur 4 amas).                   [NKTrain]
//   2) Sauve ses poids sur disque.                                [NKInfer]
//   3) Construit un modèle NEUF (poids aléatoires) -> exactitude faible.
//   4) Recharge les poids -> exactitude == modèle entraîné (round-trip exact).
//   5) Vérifie que les prédictions des deux modèles coïncident.
//   (Section MNIST optionnelle : NK_MNIST_DIR.)
// =============================================================================
#include "NKInfer/NkInfer.h"
#include "NKTrain/NkTrain.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;
static void Check(bool ok, const char* name) {
    (ok ? g_pass : g_fail)++;
    printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

int main() {
    printf("=== NKInferTest : persistance des poids + inférence ===\n\n");

    const uint32 NC = 4;
    data::NkDataset train = data::MakeBlobs(NC, 40, 111u);
    data::NkDataset test  = data::MakeBlobs(NC, 15, 999u);
    data::NkDataLoader trainLoader(train, 32, true, 7u);
    data::NkDataLoader testLoader (test,  60, false, 1u);   // 1 lot = tout le test

    // Modèle A : entraîné.
    nn::NkDense a1(2, 32, 1234u), a2(32, NC, 5678u);
    auto fwdA = [&](const NkVar& x) { return a2.Forward(nn::Relu(a1.Forward(x))); };
    NkVector<NkVar> pa; a1.Parameters(pa); a2.Parameters(pa);
    optim::NkAdam adam(pa, 0.01f);
    for (int e = 0; e < 40; ++e) train::TrainEpoch(fwdA, adam, trainLoader);
    const double accA = train::Accuracy(fwdA, testLoader);
    printf("  modèle A (entraîné)      : exactitude test = %.1f%%\n", accA * 100.0);

    // Sauvegarde des poids.
    const char* path = "Build/nkinfer_model.bin";
    Check(infer::SaveParams(path, pa), "SaveParams (poids -> fichier NKMD)");

    // Modèle B : neuf (autres graines) -> devrait être mauvais.
    nn::NkDense b1(2, 32, 4444u), b2(32, NC, 8888u);
    auto fwdB = [&](const NkVar& x) { return b2.Forward(nn::Relu(b1.Forward(x))); };
    NkVector<NkVar> pb; b1.Parameters(pb); b2.Parameters(pb);
    const double accBfresh = train::Accuracy(fwdB, testLoader);
    printf("  modèle B (neuf/aléatoire): exactitude test = %.1f%%\n", accBfresh * 100.0);

    // Rechargement des poids de A dans B.
    Check(infer::LoadParams(path, pb), "LoadParams (fichier -> poids du modèle neuf)");
    const double accBloaded = train::Accuracy(fwdB, testLoader);
    printf("  modèle B (rechargé)      : exactitude test = %.1f%%\n", accBloaded * 100.0);

    Check(accBloaded == accA, "Round-trip : B rechargé == A (exactitude identique)");
    Check(accBfresh < accA,   "Le chargement change bien le modèle (neuf < entraîné)");

    // Les prédictions des deux modèles doivent coïncider exactement.
    data::NkBatch tb = testLoader.GetBatch(0);
    NkVar x = NkVar::Leaf(tb.inputs, false);
    NkVector<int32> predA = infer::Predict(fwdA(x).Value());
    NkVector<int32> predB = infer::Predict(fwdB(x).Value());
    bool same = predA.Size() == predB.Size();
    for (uint32 i = 0; same && i < predA.Size(); ++i) if (predA[i] != predB[i]) same = false;
    Check(same, "Prédictions A == B après rechargement");

    // ------------------------------------------------------------------
    printf("\n-- MNIST (optionnel) --\n");
    const char* dir = getenv("NK_MNIST_DIR");
    if (!dir || !*dir) {
        printf("  (NK_MNIST_DIR non défini : section MNIST ignorée)\n");
    } else {
        char img[1024], lbl[1024];
        snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", dir);
        snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", dir);
        data::NkDataset mnist = data::LoadMnist(img, lbl);
        if (!mnist.IsValid()) { printf("  MNIST introuvable (ignoré)\n"); }
        else {
            data::NkDataLoader ml(mnist, 64, true, 3u);
            nn::NkDense m1(784, 64, 11u), m2(64, 10, 22u);
            auto mfwd = [&](const NkVar& x2) { return m2.Forward(nn::Relu(m1.Forward(x2))); };
            NkVector<NkVar> mp; m1.Parameters(mp); m2.Parameters(mp);
            optim::NkAdam mopt(mp, 0.001f);
            printf("  entraînement MLP 784->64->10 (3 époques)...\n");
            for (int e = 1; e <= 3; ++e) {
                train::EpochStats s = train::TrainEpoch(mfwd, mopt, ml);
                printf("    époque %d : perte %.4f  exactitude %.1f%%\n", e, s.loss, s.acc * 100.0);
            }
            infer::SaveParams("Build/nkinfer_mnist.bin", mp);
            printf("  poids MNIST sauvés (Build/nkinfer_mnist.bin)\n");
        }
    }

    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
