// =============================================================================
// NKGptTrain — petit GPT char-level, from-scratch, entraîné 100% GPU-résident,
// qui GÉNÈRE du texte. Assemble tout NKAI : NkGPT (NKNN) + AdamW (NKOptim) +
// softmax-CE (NKAutograd), sur un corpus texte réel (Project Gutenberg).
//
//   Brique 9  : tokenizer char-level (vocab = octets présents ; encode/decode).
//   Brique 10 : boucle d'entraînement + échantillonnage autoregressif.
//
// Corpus : NK_GPT_FILE (défaut = un livre français du dossier Datasets).
// Étapes : NK_GPT_STEPS (défaut 300).
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

using namespace nkentseu;
using namespace nkentseu::ai;

// ---- Corpus : lit un fichier, saute l'entête Gutenberg, cape à maxChars -----
static std::string LoadCorpus(const std::string& path, size_t maxChars) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::string();
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Saute l'entête Project Gutenberg si présente.
    size_t start = 0;
    size_t m = all.find("*** START OF");
    if (m != std::string::npos) { size_t nl = all.find('\n', m); if (nl != std::string::npos) start = nl + 1; }
    size_t end = all.find("*** END OF");
    std::string body = all.substr(start, (end != std::string::npos && end > start) ? end - start : std::string::npos);
    if (body.size() > maxChars) body = body.substr(0, maxChars);
    return body;
}

int main() {
    printf("=== NKGptTrain : petit GPT char-level (from-scratch, GPU-résident) ===\n");
    NkTensorGpu& gpu = NkTensorGpu::Get();
    const bool useGpu = gpu.IsAvailable();
    printf("GPU compute : %s (%s)\n", useGpu ? "OUI" : "NON", gpu.BackendName());

    // ---- Corpus ----
    const char* envf = getenv("NK_GPT_FILE");
    std::string path = envf ? envf
        : "D:/Projets/2026/Nkentseu/Nkentseu/Resources/Datasets/pg17989.txt"; // Comte de Monte-Cristo (FR)
    const char* envc = getenv("NK_GPT_CHARS");
    size_t maxChars = envc ? (size_t)atol(envc) : 150000;
    std::string text = LoadCorpus(path, maxChars);
    if (text.size() < 1000) { printf("Corpus introuvable/trop court : %s\n", path.c_str()); return 2; }

    // ---- Brique 9 : tokenizer char-level (vocab = octets présents) ----
    int stoi[256]; for (int i = 0; i < 256; ++i) stoi[i] = -1;
    std::vector<unsigned char> itos;
    for (unsigned char c : text) if (stoi[c] < 0) { stoi[c] = (int)itos.size(); itos.push_back(c); }
    const int V = (int)itos.size();
    std::vector<float> data; data.reserve(text.size());
    for (unsigned char c : text) data.push_back((float)stoi[c]);
    printf("Corpus : %zu caractères, vocabulaire = %d symboles distincts.\n", text.size(), V);

    // ---- Modèle (réglable par env : NK_GPT_D/H/L/T/B) ----
    auto envI = [](const char* k, int64 def) -> int64 { const char* v = getenv(k); return v ? (int64)atol(v) : def; };
    const int64 T = envI("NK_GPT_T", 128);   // contexte
    const int64 d = envI("NK_GPT_D", 256);   // dimension modèle
    const int64 H = envI("NK_GPT_H", 8);     // têtes d'attention
    const int64 L = envI("NK_GPT_L", 4);     // couches transformer
    const int64 B = envI("NK_GPT_B", 16);    // taille de lot
    printf("Modèle GPT : T=%lld, d=%lld, têtes=%lld, couches=%lld, batch=%lld  (AdamW, GPU-résident)\n\n",
           (long long)T,(long long)d,(long long)H,(long long)L,(long long)B);
    nn::NkGPT gpt((uint32)V, (uint32)d, (uint32)H, (uint32)L, (uint32)T, 1234u);
    NkVector<NkVar> params; gpt.Parameters(params);
    if (useGpu) for (uint32 i = 0; i < params.Size(); ++i) params[i].SetValue(params[i].Value().ToGPU());
    optim::NkAdam adam(params, 3e-4f, 0.9f, 0.999f, 1e-8f, /*weightDecay=AdamW*/ 0.01f);

    // RNG déterministe (LCG) pour échantillonner batches et génération.
    uint64 rng = 0x9E3779B97F4A7C15ull;
    auto nextRand = [&rng]() { rng = rng * 6364136223846793005ull + 1442695040888963407ull; return (double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52); };

    // Fabrique un lot : x[B,T], cible one-hot [B*T, V] (caractère suivant).
    auto makeBatch = [&](NkTensor& x, NkTensor& oneHot) {
        NkShape xs; xs.PushBack(B); xs.PushBack(T);
        x = NkTensor::Zeros(xs);
        oneHot = NkTensor::Zeros(NkShape{ B * T, (int64)V });
        float* xp = x.DataAs<float>(); float* op = oneHot.DataAs<float>();
        const int64 N = (int64)data.size();
        for (int64 b = 0; b < B; ++b) {
            int64 off = (int64)(nextRand() * (double)(N - T - 1));
            for (int64 t = 0; t < T; ++t) {
                xp[b*T + t] = data[off + t];
                int tgt = (int)data[off + t + 1];
                op[(b*T + t) * V + tgt] = 1.f;
            }
        }
    };

    // Génération autoregressive (température) depuis une amorce.
    auto generate = [&](const std::string& seed, int nChars, double temp) -> std::string {
        std::vector<int> ctx;
        for (unsigned char c : seed) if (stoi[c] >= 0) ctx.push_back(stoi[c]);
        if (ctx.empty()) ctx.push_back(0);
        std::string out = seed;
        std::vector<float> logitBuf(V);
        for (int i = 0; i < nChars; ++i) {
            int64 len = (int64)ctx.size(); if (len > T) len = T;
            NkTensor tok = NkTensor::Zeros(NkShape{ (int64)1, len });
            float* tp = tok.DataAs<float>();
            for (int64 t = 0; t < len; ++t) tp[t] = (float)ctx[ctx.size() - len + t];
            NkVar logits = gpt.Forward(useGpu ? tok.ToGPU() : tok);   // [len, V]
            NkTensor lc = logits.Value().ToCPU().Contiguous();
            const float* lp = lc.DataAs<float>() + (len - 1) * V;     // dernière position
            // softmax(logits/temp) puis échantillonnage.
            double mx = lp[0]; for (int v = 1; v < V; ++v) if (lp[v] > mx) mx = lp[v];
            double sum = 0; for (int v = 0; v < V; ++v) { double e = std::exp((lp[v] - mx) / temp); logitBuf[v] = (float)e; sum += e; }
            double r = nextRand() * sum, acc = 0; int next = V - 1;
            for (int v = 0; v < V; ++v) { acc += logitBuf[v]; if (acc >= r) { next = v; break; } }
            ctx.push_back(next);
            out.push_back((char)itos[next]);
        }
        return out;
    };

    // ---- Brique 10 : entraînement ----
    const char* envs = getenv("NK_GPT_STEPS");
    const int STEPS = envs ? atoi(envs) : 300;
    printf("-- Entraînement (%d pas) --\n", STEPS);
    double ema = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int s = 1; s <= STEPS; ++s) {
        NkTensor x, oneHot; makeBatch(x, oneHot);
        NkVar logits = gpt.Forward(useGpu ? x.ToGPU() : x);           // [B*T, V]
        NkVar loss = autograd::SoftmaxCrossEntropy(logits, NkVar::Leaf(useGpu ? oneHot.ToGPU() : oneHot, false));
        loss.Backward(); adam.Step(); adam.ZeroGrad();
        double lv = loss.Value().ToCPU().GetItem(NkShape{ (int64)0 });
        ema = (s == 1) ? lv : 0.98 * ema + 0.02 * lv;
        if (s % 25 == 0 || s == 1) printf("  pas %4d : perte = %.4f  (moy. %.4f)\n", s, lv, ema);
        if (s % 100 == 0) {
            std::string g = generate("Le ", 160, 0.8);
            printf("    --- échantillon (pas %d) ---\n    %s\n    ---------------------------\n", s, g.c_str());
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    printf("Entraînement terminé en %.1f s (%s).\n", std::chrono::duration<double>(t1 - t0).count(), useGpu ? "GPU-résident" : "CPU");

    // ---- Génération finale ----
    printf("\n=== TEXTE GÉNÉRÉ (amorce « Le », 400 car., temp 0.8) ===\n");
    printf("%s\n", generate("Le ", 400, 0.8).c_str());
    printf("=========================================================\n");

    bool ok = ema < 3.0;   // la perte a nettement baissé depuis ~ln(V)
    printf("\n[%s] le GPT a appris (perte %.2f) et génère du texte %s.\n",
           ok ? " OK " : "FAIL", ema, useGpu ? "100%% sur GPU" : "sur CPU");
    gpu.Shutdown();
    return ok ? 0 : 1;
}
