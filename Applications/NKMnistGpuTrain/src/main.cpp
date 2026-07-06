// =============================================================================
// NKMnistGpuTrain — entraînement MNIST RÉEL de bout en bout, GPU-RÉSIDENT.
//   MLP 784->256->10 (NKNN), entropie croisée softmax, Adam FUSÉ (NKOptim),
//   boucle TrainEpoch (NKTrain), données réelles (NKData / IDX).
// Tous les paramètres sont déplacés sur GPU et chaque lot y est envoyé : forward,
// backward et pas d'optimiseur restent résidents. Valide la pile IA sur une vraie
// tâche. (Définir NK_MNIST_DIR pour un autre chemin ; défaut = Datasets/mnist du dépôt.)
// =============================================================================
#include "NKTrain/NkTrain.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
    printf("=== NKMnistGpuTrain : MNIST réel, entraînement GPU-résident ===\n");
    NkTensorGpu& gpu = NkTensorGpu::Get();
    const bool useGpu = gpu.IsAvailable();
    printf("GPU compute : %s (%s)\n", useGpu ? "OUI" : "NON", gpu.BackendName());

    const char* env = getenv("NK_MNIST_DIR");
    const char* base = (env && *env) ? env : "D:/Projets/2026/Nkentseu/Nkentseu/Datasets/mnist";
    char img[1024], lbl[1024];
    snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", base);
    snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", base);
    data::NkDataset mnist = data::LoadMnist(img, lbl);
    if (!mnist.IsValid()) { printf("MNIST introuvable dans %s. Abandon.\n", base); return 2; }
    printf("MNIST chargé : %u exemples. Modèle 784->256->10, Adam lr=0.001, batch=128.\n\n", mnist.Size());

    data::NkDataLoader loader(mnist, 128, /*shuffle*/ true, 3u);

    nn::NkDense m1(784, 256, 11u);
    nn::NkDense m2(256, 10, 22u);
    NkVector<NkVar> params; m1.Parameters(params); m2.Parameters(params);

    // Résidence : basculer TOUS les paramètres sur GPU (mise à jour en place du nœud).
    if (useGpu)
        for (uint32 i = 0; i < params.Size(); ++i)
            params[i].SetValue(params[i].Value().ToGPU());

    // Forward : le lot d'entrée est aussi envoyé sur GPU -> chaîne entièrement résidente.
    auto fwd = [&](const NkVar& x) {
        NkVar xg = useGpu ? NkVar::Leaf(x.Value().ToGPU(), false) : x;
        return m2.Forward(nn::Relu(m1.Forward(xg)));
    };

    optim::NkAdam adam(params, 0.001f);   // params GPU -> pas d'Adam FUSÉ

    const int EPOCHS = 3;
    train::EpochStats st;
    for (int e = 1; e <= EPOCHS; ++e) {
        auto t0 = std::chrono::high_resolution_clock::now();
        st = train::TrainEpoch(fwd, adam, loader);
        auto t1 = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(t1 - t0).count();
        printf("  époque %d : perte = %.4f  exactitude train = %.2f%%   (%.1f s, %s)\n",
               e, st.loss, st.acc * 100.0, sec, useGpu ? "GPU-résident" : "CPU");
    }

    double accTrain = train::Accuracy(fwd, loader);

    // Généralisation : jeu de TEST (t10k, jamais vu à l'entraînement).
    char timg[1024], tlbl[1024];
    snprintf(timg, sizeof(timg), "%s/t10k-images-idx3-ubyte", base);
    snprintf(tlbl, sizeof(tlbl), "%s/t10k-labels-idx1-ubyte", base);
    data::NkDataset test = data::LoadMnist(timg, tlbl);
    double accTest = -1.0;
    if (test.IsValid()) {
        data::NkDataLoader testLoader(test, 128, false, 1u);
        accTest = train::Accuracy(fwd, testLoader);
    }

    printf("\n  exactitude train = %.2f%%", accTrain * 100.0);
    if (accTest >= 0.0) printf("   |   TEST (jamais vu, %u ex.) = %.2f%%", test.Size(), accTest * 100.0);
    printf("\n");
    bool ok = accTrain >= 0.90 && (accTest < 0.0 || accTest >= 0.90);
    printf("  [%s] classifieur MNIST réel entraîné %s (>=90%% train & test)\n",
           ok ? " OK " : "FAIL", useGpu ? "ENTIÈREMENT sur GPU" : "sur CPU");
    gpu.Shutdown();
    return ok ? 0 : 1;
}
