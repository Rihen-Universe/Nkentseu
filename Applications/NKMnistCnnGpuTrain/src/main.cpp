// =============================================================================
// NKMnistCnnGpuTrain — CNN MNIST RÉEL de bout en bout, 100% GPU-RÉSIDENT.
//   x[B,784] -> [B,1,28,28]
//     Conv2D(1->8,3x3,pad1)+ReLU -> MaxPool2 -> [B,8,14,14]
//     Conv2D(8->16,3x3,pad1)+ReLU -> MaxPool2 -> [B,16,7,7]
//     flatten [B,784] -> Dense(784->10) -> SoftmaxCE
// Conv (im2col/col2im), MaxPool, ReLU, Dense, softmax-CE et Adam FUSÉ : tout sur GPU.
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
	printf("=== NKMnistCnnGpuTrain : CNN MNIST réel, entraînement GPU-résident ===\n");
	NkTensorGpu &gpu = NkTensorGpu::Get();
	const bool useGpu = gpu.IsAvailable();
	printf("GPU compute : %s (%s)\n", useGpu ? "OUI" : "NON", gpu.BackendName());

	const char *envd = getenv("NK_MNIST_DIR");
	const char *base = (envd && *envd) ? envd : "D:/Projets/2026/Nkentseu/Nkentseu/Datasets/mnist";
	char img[1024], lbl[1024];
	snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", base);
	snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", base);
	data::NkDataset mnist = data::LoadMnist(img, lbl);
	if (!mnist.IsValid()) {
		printf("MNIST introuvable dans %s. Abandon.\n", base);
		return 2;
	}
	printf("MNIST chargé : %u exemples. CNN (2 conv + maxpool + dense), Adam lr=0.001, batch=128.\n\n", mnist.Size());

	data::NkDataLoader loader(mnist, 128, /*shuffle*/ true, 3u);

	nn::NkConv2D conv1(1, 8, 3, 1, 1, 101u);  // [B,1,28,28] -> [B,8,28,28]
	nn::NkConv2D conv2(8, 16, 3, 1, 1, 202u); // [B,8,14,14] -> [B,16,14,14]
	nn::NkDense dense(16 * 7 * 7, 10, 303u);  // 784 -> 10
	NkVector<NkVar> params;
	conv1.Parameters(params);
	conv2.Parameters(params);
	dense.Parameters(params);

	// Résidence : tous les paramètres sur GPU.
	if (useGpu)
		for (uint32 i = 0; i < params.Size(); ++i)
			params[i].SetValue(params[i].Value().ToGPU());

	// Forward CNN complet (résident si useGpu). B = taille réelle du lot.
	auto fwd = [&](const NkVar &x) {
		const int64 B = x.Value().Shape()[0];
		NkVar xg = useGpu ? NkVar::Leaf(x.Value().ToGPU(), false) : x;
		NkVar x4 = autograd::Reshape(xg, NkShape{B, 1, 28, 28});
		NkVar h1 = nn::MaxPool2D(nn::Relu(conv1.Forward(x4)), 2, 2); // [B,8,14,14]
		NkVar h2 = nn::MaxPool2D(nn::Relu(conv2.Forward(h1)), 2, 2); // [B,16,7,7]
		NkVar f = autograd::Reshape(h2, NkShape{B, 16 * 7 * 7});
		return dense.Forward(f);
	};

	optim::NkAdam adam(params, 0.001f);

	const int EPOCHS = 1; // le CNN converge vite ; 1 époque suffit à démontrer (debug = lent)
	train::EpochStats st;
	for (int e = 1; e <= EPOCHS; ++e) {
		auto t0 = std::chrono::high_resolution_clock::now();
		st = train::TrainEpoch(fwd, adam, loader);
		auto t1 = std::chrono::high_resolution_clock::now();
		double sec = std::chrono::duration<double>(t1 - t0).count();
		printf("  époque %d : perte = %.4f  exactitude train = %.2f%%   (%.1f s, %s)\n", e, st.loss, st.acc * 100.0,
			   sec, useGpu ? "GPU-résident" : "CPU");
	}

	// Test (t10k, jamais vu).
	char timg[1024], tlbl[1024];
	snprintf(timg, sizeof(timg), "%s/t10k-images-idx3-ubyte", base);
	snprintf(tlbl, sizeof(tlbl), "%s/t10k-labels-idx1-ubyte", base);
	data::NkDataset test = data::LoadMnist(timg, tlbl);
	double accTest = -1.0;
	if (test.IsValid()) {
		data::NkDataLoader testLoader(test, 128, false, 1u);
		accTest = train::Accuracy(fwd, testLoader);
	}

	printf("\n  exactitude train = %.2f%%", st.acc * 100.0);
	if (accTest >= 0.0)
		printf("   |   TEST (jamais vu, %u ex.) = %.2f%%", test.Size(), accTest * 100.0);
	printf("\n");
	bool ok = st.acc >= 0.85 && (accTest < 0.0 || accTest >= 0.85);
	printf("  [%s] CNN MNIST réel entraîné %s (>=85%% en 1 époque)\n", ok ? " OK " : "FAIL",
		   useGpu ? "ENTIÈREMENT sur GPU" : "sur CPU");
	gpu.Shutdown();
	return ok ? 0 : 1;
}
