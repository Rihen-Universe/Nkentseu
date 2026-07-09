// =============================================================================
// NKGptTrain — petit GPT BPE, from-scratch, entraîné 100% GPU-résident, qui GÉNÈRE
// du texte. Assemble tout NKAI : NkGPT (NKNN) + AdamW (NKOptim) + softmax-CE
// (NKAutograd) sur un corpus texte réel, tokenisé en BPE.
//
//   Tokenizer : BPE from-scratch (256 octets + fusions ; NK_GPT_MERGES, défaut 600)
//               → le modèle manipule des morceaux de mots (texte plus lisible).
//   Entraînement : boucle + échantillonnage autoregressif, tag de langue.
//
// ZÉRO-STL : conteneurs/chaînes/fichiers/temps = maison (NkString, NkVector, NkFile,
//            NkDirectory, NkChrono). Seuls FILE*/fread (C, comme NKInfer) et exp()
//            (C math) sont utilisés — pas de std:: conteneurs/algorithmes.
//
// Corpus  : NK_GPT_DIR (défaut = tout Datasets) ou NK_GPT_FILE (un livre).
// Taille  : NK_GPT_T/D/H/L/B, NK_GPT_MERGES ; étapes : NK_GPT_STEPS (défaut 300).
// Modèle  : NK_GPT_SAVE=chemin (sauve après entraînement),
//           NK_GPT_LOAD=chemin (recharge + génère SANS réentraîner),
//           NK_GPT_PROMPT="amorce", NK_GPT_GENLEN=nb tokens générés,
//           NK_GPT_LANG=fr|en|bbj (pilote la langue via le tag de langue).
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"
#include "NKGpt/NkGptCore.h"   // briques réutilisables : BPE, corpus, checkpoint (module NKGpt)

#include <cstdio>
#include <cstdlib>   // getenv, atol, atoi
#include <math.h>    // exp

using namespace nkentseu;
using namespace nkentseu::ai;
using namespace nkentseu::ai::gpt;   // LoadCorpus/LangOf/LoadCorpusByLang/Bpe/TrainBpe/GptMeta/*Checkpoint*

// (BPE, corpus, checkpoint = module NKGpt : Kernel/AI/NKGpt/src/NKGpt/NkGptCore.h)

int main() {
    printf("=== NKGptTrain : petit GPT BPE (from-scratch, GPU-résident, zéro-STL) ===\n");
    NkTensorGpu& gpu = NkTensorGpu::Get();
    const bool useGpu = gpu.IsAvailable();
    printf("GPU compute : %s (%s)\n", useGpu ? "OUI" : "NON", gpu.BackendName());

    auto envI = [](const char* k, int64 def) -> int64 { const char* v = getenv(k); return v ? (int64)atol(v) : def; };
    const char* envLoad   = getenv("NK_GPT_LOAD");
    const char* envSave   = getenv("NK_GPT_SAVE");
    const char* envPrompt = getenv("NK_GPT_PROMPT");
    const NkString seed = envPrompt ? NkString(envPrompt) : NkString("Le ");

    Bpe bpe;
    NkVector<NkString> langs;
    NkVector<NkVector<float>> langData;               // ids BPE par langue (train)
    NkVector<NkVector<float>> langMask;               // parallèle : 1=compte dans la loss, 0=masqué (question/séparateur)
    int V = 0, nByte = 0;
    int64 T = 0, d = 0, H = 0, L = 0, B = envI("NK_GPT_B", 16);

    // Reprise d'entraînement : NK_GPT_LOAD (checkpoint) + NK_GPT_RESUME=1 -> on recharge les
    // poids ET on continue à entraîner (au lieu de seulement générer). L'état Adam n'est PAS
    // sauvegardé (optimiseur neuf à la reprise) ; les POIDS, eux, sont préservés.
    const bool resume = envLoad && (envI("NK_GPT_RESUME", 0) != 0);

    // Encodage d'un corpus (texts par langue) -> langData/langMask, avec le BPE courant
    // (celui du checkpoint en reprise, celui fraîchement entraîné sinon). Masque la question
    // des blocs Question:/Réponse: (instruction-tuning). Réutilisé fresh ET resume.
    const NkStringView marker("Réponse: ");
    auto encodeCorpus = [&](const NkVector<NkString>& texts) -> nk_size {
        langData.Resize((nk_size)texts.Size());
        langMask.Resize((nk_size)texts.Size());
        nk_size totalTok = 0;
        for (int64 li = 0; li < (int64)texts.Size(); ++li) {
            const NkString& txt = texts[(nk_size)li];
            const bool isQa = txt.Find(marker) != NkString::npos;
            if (!isQa) {
                NkVector<int32> ids; bpe.Encode(txt, ids);
                for (int64 k = 0; k < (int64)ids.Size(); ++k) {
                    langData[(nk_size)li].PushBack((float)ids[(nk_size)k]);
                    langMask[(nk_size)li].PushBack(1.f);
                }
            } else {
                const nk_size sz = txt.Size();
                nk_size pos = 0;
                while (pos < sz) {
                    const nk_size be = txt.Find("\n\n", pos);
                    const nk_size blen = (be == NkString::npos) ? (sz - pos) : (be - pos);
                    NkString block = txt.SubStr(pos, blen);
                    pos = (be == NkString::npos) ? sz : be + 2;
                    if (block.Size() == 0) continue;
                    const nk_size mp = block.Find(marker);
                    NkString qPart = (mp == NkString::npos) ? block : block.SubStr(0, mp + marker.Size());
                    NkVector<int32> qIds; bpe.Encode(qPart, qIds);
                    for (int64 k = 0; k < (int64)qIds.Size(); ++k) { langData[(nk_size)li].PushBack((float)qIds[(nk_size)k]); langMask[(nk_size)li].PushBack(0.f); }
                    if (mp != NkString::npos) {
                        NkString aPart = block.SubStr(mp + marker.Size());
                        if (aPart.Size() > 0) {
                            NkVector<int32> aIds; bpe.Encode(aPart, aIds);
                            for (int64 k = 0; k < (int64)aIds.Size(); ++k) { langData[(nk_size)li].PushBack((float)aIds[(nk_size)k]); langMask[(nk_size)li].PushBack(1.f); }
                        }
                    }
                    NkVector<int32> sepIds; bpe.Encode(NkString("\n\n"), sepIds);
                    for (int64 k = 0; k < (int64)sepIds.Size(); ++k) { langData[(nk_size)li].PushBack((float)sepIds[(nk_size)k]); langMask[(nk_size)li].PushBack(0.f); }
                }
            }
            totalTok += langData[(nk_size)li].Size();
        }
        return totalTok;
    };

    if (envLoad) {
        GptMeta meta;
        if (!LoadCheckpointMeta(envLoad, meta)) { printf("Checkpoint illisible ou format obsolète (attendu BPE v3) : %s\n", envLoad); return 2; }
        V = meta.V; d = meta.d; H = meta.H; L = meta.L; T = meta.T;
        langs = meta.langs;
        for (int64 i = 0; i < (int64)meta.merges.Size(); ++i) bpe.merges.PushBack(meta.merges[(nk_size)i]);
        bpe.BuildVocabRank();
        nByte = bpe.Base();
        printf("Modèle chargé : %s (V=%d, T=%lld, d=%lld, têtes=%lld, couches=%lld, %llu fusions BPE)\n",
               envLoad, V, (long long)T, (long long)d, (long long)H, (long long)L, (unsigned long long)bpe.merges.Size());
        if (langs.Size() > 0) { printf("Langues (NK_GPT_LANG) :"); for (int64 i = 0; i < (int64)langs.Size(); ++i) printf(" %s", langs[(nk_size)i].CStr()); printf("\n"); }
    } else {
        const char* envf = getenv("NK_GPT_FILE");
        const char* envd = getenv("NK_GPT_DIR");
        const char* envc = getenv("NK_GPT_CHARS");
        const NkString datasetsDir = envd ? NkString(envd) : NkString("D:/Projets/2026/Nkentseu/Nkentseu/Resources/Datasets");
        NkVector<NkString> texts;
        if (envf) {
            nk_size maxChars = envc ? (nk_size)atol(envc) : 150000;
            printf("Corpus : fichier unique %s\n", envf);
            langs.PushBack(LangOf(NkString(envf)));
            texts.PushBack(LoadCorpus(envf, maxChars));
        } else {
            nk_size totalCap = envc ? (nk_size)atol(envc) : 1200000;
            printf("Corpus : dossier %s (équilibré par langue, cap total %llu)\n", datasetsDir.CStr(), (unsigned long long)totalCap);
            LoadCorpusByLang(datasetsDir, totalCap, langs, texts);
        }
        nk_size totalChars = 0; for (int64 i = 0; i < (int64)texts.Size(); ++i) totalChars += texts[(nk_size)i].Size();
        if (totalChars < 1000) { printf("Corpus introuvable/trop court.\n"); return 2; }
        const int nMerges = (int)envI("NK_GPT_MERGES", 600);
        printf("Entraînement du tokenizer BPE (%d fusions cible)...\n", nMerges);
        TrainBpe(texts, nMerges, bpe);
        nByte = bpe.Base();
        V = nByte + (int)langs.Size();
        const nk_size totalTok = encodeCorpus(texts);
        printf("Corpus : %llu car. -> %llu tokens BPE ; %d tokens (256 + %llu fusions) + %d tags = vocab %d.\n",
               (unsigned long long)totalChars, (unsigned long long)totalTok, nByte, (unsigned long long)bpe.merges.Size(), (int)langs.Size(), V);
        T = envI("NK_GPT_T", 128); d = envI("NK_GPT_D", 256);
        H = envI("NK_GPT_H", 8);   L = envI("NK_GPT_L", 4);
        printf("Modèle GPT : T=%lld, d=%lld, têtes=%lld, couches=%lld, batch=%lld  (AdamW, GPU-résident)\n\n",
               (long long)T,(long long)d,(long long)H,(long long)L,(long long)B);
    }

    // ---- REPRISE D'ENTRAÎNEMENT : charger le corpus avec le BPE du checkpoint ----
    // (les dims + langues + fusions BPE viennent du checkpoint ; on ré-encode le corpus
    //  et on continuera à entraîner. Le corpus DOIT exposer les mêmes tags dans le même
    //  ordre que le checkpoint, sinon les tokens-tag de langue ne correspondraient plus.)
    if (resume) {
        const char* envf = getenv("NK_GPT_FILE");
        const char* envd = getenv("NK_GPT_DIR");
        const char* envc = getenv("NK_GPT_CHARS");
        const NkString datasetsDir = envd ? NkString(envd) : NkString("D:/Projets/2026/Nkentseu/Nkentseu/Resources/Datasets");
        NkVector<NkString> texts;
        NkVector<NkString> langs2;
        if (envf) {
            langs2.PushBack(LangOf(NkString(envf)));
            texts.PushBack(LoadCorpus(envf, envc ? (nk_size)atol(envc) : 150000));
        } else {
            LoadCorpusByLang(datasetsDir, envc ? (nk_size)atol(envc) : 1200000, langs2, texts);
        }
        // Les langues du corpus doivent correspondre EXACTEMENT à celles du checkpoint.
        bool langsOk = (langs2.Size() == langs.Size());
        for (int64 i = 0; langsOk && i < (int64)langs.Size(); ++i)
            if (!(langs2[(nk_size)i] == langs[(nk_size)i])) langsOk = false;
        if (!langsOk) {
            printf("Reprise IMPOSSIBLE : les tags du corpus ne correspondent pas au checkpoint "
                   "(mêmes fichiers/tags, même ordre requis).\n");
            gpu.Shutdown();
            return 2;
        }
        const nk_size totalTok = encodeCorpus(texts);
        printf("Reprise : corpus ré-encodé avec le BPE du checkpoint (%llu tokens BPE, %d tags).\n",
               (unsigned long long)totalTok, (int)langs.Size());
    }

    // Langue de génération demandée (NK_GPT_LANG=fr/en/bbj) — -1 = auto (pas de tag).
    const char* envLang = getenv("NK_GPT_LANG");
    int genLang = -1;
    if (envLang) for (int64 i = 0; i < (int64)langs.Size(); ++i) if (langs[(nk_size)i] == envLang) { genLang = (int)i; break; }

    // ---- Construction + (chargement des poids | init aléatoire) ----
    nn::NkGPT gpt((uint32)V, (uint32)d, (uint32)H, (uint32)L, (uint32)T, 1234u);
    NkVector<NkVar> params; gpt.Parameters(params);
    if (envLoad) {
        if (!LoadCheckpointWeights(envLoad, params)) { printf("Poids du checkpoint incompatibles avec les dims.\n"); return 2; }
        printf("Poids rechargés (%u tenseurs).\n", params.Size());
    }
    if (useGpu) for (uint32 i = 0; i < params.Size(); ++i) params[i].SetValue(params[i].Value().ToGPU());

    // RNG déterministe (LCG).
    uint64 rng = 0x9E3779B97F4A7C15ull;
    auto nextRand = [&rng]() { rng = rng * 6364136223846793005ull + 1442695040888963407ull; return (double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52); };

    // Lot : x[B,T], cible one-hot [B*T, V]. Chaque séquence commence par le TAG de sa
    // langue (position 0) ; langues en round-robin sur le lot.
    auto makeBatch = [&](NkTensor& x, NkTensor& oneHot) {
        NkShape xs; xs.PushBack(B); xs.PushBack(T);
        x = NkTensor::Zeros(xs);
        oneHot = NkTensor::Zeros(NkShape{ B * T, (int64)V });
        float* xp = x.DataAs<float>(); float* op = oneHot.DataAs<float>();
        const int nL = (int)langData.Size();
        for (int64 b = 0; b < B; ++b) {
            const int li = nL > 0 ? (int)(b % nL) : 0;
            const NkVector<float>& dd = langData[(nk_size)li];
            // Masque parallèle (si présent et cohérent) ; sinon tout compte (comportement d'origine).
            const bool hasMask = ((nk_size)li < langMask.Size()) && (langMask[(nk_size)li].Size() == dd.Size());
            const int64 N = (int64)dd.Size();
            if (N <= T) continue;
            const int64 off = (int64)(nextRand() * (double)(N - T));
            xp[b*T + 0] = (float)(nByte + li);
            // Cible d'une position = token suivant ; ligne laissée à 0 (masquée) si masque=0.
            if (!hasMask || langMask[(nk_size)li][(nk_size)off] != 0.f)
                op[(b*T + 0) * V + (int)dd[(nk_size)off]] = 1.f;
            for (int64 t = 1; t < T; ++t) {
                xp[b*T + t] = dd[(nk_size)(off + t - 1)];
                if (!hasMask || langMask[(nk_size)li][(nk_size)(off + t)] != 0.f)
                    op[(b*T + t) * V + (int)dd[(nk_size)(off + t)]] = 1.f;
            }
        }
    };

    // Génération autoregressive (température). langIdx>=0 => préfixe le tag de langue.
    auto generate = [&](const NkString& sd, int nToks, double temp, int langIdx) -> NkString {
        NkVector<int32> ctx;
        if (langIdx >= 0 && langIdx < (int)langs.Size()) ctx.PushBack((int32)(nByte + langIdx));
        NkVector<int32> seedIds; bpe.Encode(sd, seedIds);
        for (int64 i = 0; i < (int64)seedIds.Size(); ++i) ctx.PushBack(seedIds[(nk_size)i]);
        if (ctx.Size() == 0) ctx.PushBack(0);
        NkString out = sd;
        NkVector<float> logitBuf; logitBuf.Resize((nk_size)V);
        for (int i = 0; i < nToks; ++i) {
            int64 len = (int64)ctx.Size(); if (len > T) len = T;
            NkTensor tok = NkTensor::Zeros(NkShape{ (int64)1, len });
            float* tp = tok.DataAs<float>();
            for (int64 t = 0; t < len; ++t) tp[t] = (float)ctx[(nk_size)((int64)ctx.Size() - len + t)];
            NkVar logits = gpt.Forward(useGpu ? tok.ToGPU() : tok);
            NkTensor lc = logits.Value().ToCPU().Contiguous();
            const float* lp = lc.DataAs<float>() + (len - 1) * V;
            double mx = -1e30; for (int v = 0; v < nByte; ++v) if (lp[v] > mx) mx = lp[v];
            double sum = 0;
            for (int v = 0; v < V; ++v) {
                if (v >= nByte) { logitBuf[(nk_size)v] = 0.f; continue; }
                double e = exp((lp[v] - mx) / temp); logitBuf[(nk_size)v] = (float)e; sum += e;
            }
            double r = nextRand() * sum, acc = 0; int next = 0;
            for (int v = 0; v < nByte; ++v) { acc += logitBuf[(nk_size)v]; if (acc >= r) { next = v; break; } }
            ctx.PushBack((int32)next);
            out.Append(bpe.Decode(next));
        }
        return out;
    };

    const int GENLEN = (int)envI("NK_GPT_GENLEN", 400);

    // ---- Mode CHARGEMENT : on génère et on sort (SAUF si reprise d'entraînement) ----
    if (envLoad && !resume) {
        printf("\n=== TEXTE GÉNÉRÉ (langue %s, amorce « %s », %d tokens) ===\n",
               genLang >= 0 ? langs[(nk_size)genLang].CStr() : "auto", seed.CStr(), GENLEN);
        printf("%s\n", generate(seed, GENLEN, 0.8, genLang).CStr());
        printf("=========================================================\n");
        gpu.Shutdown();
        return 0;
    }

    // ---- Entraînement ----
    const char* envs = getenv("NK_GPT_STEPS");
    const int STEPS = envs ? atoi(envs) : 300;
    // Accumulation de gradient : ACCUM micro-lots -> batch EFFECTIF = B*ACCUM, avec la
    // mémoire d'activations d'UN SEUL micro-lot (B). Levier n°1 pour tenir un gros modèle
    // sur une VRAM limitée sans réduire la qualité de gradient. NK_GPT_ACCUM (défaut 1).
    const int ACCUM = (int)envI("NK_GPT_ACCUM", 1);
    // LR schedule : warmup linéaire (NK_GPT_WARMUP, défaut 5% des pas) puis décroissance
    // cosine jusqu'à 10% du pic. Stabilise et accélère la convergence sur les longs runs.
    const char* envlr = getenv("NK_GPT_LR");
    const float peakLr = envlr ? (float)atof(envlr) : 3e-4f;
    const int WARMUP = (int)envI("NK_GPT_WARMUP", STEPS / 20);
    const double kPi = 3.14159265358979323846;
    const float minLrRatio = 0.1f;
    // Checkpoint périodique : sauvegarde tous les NK_GPT_SAVEEVERY pas (0 = seulement à la
    // fin). Indispensable sur un run long : un plantage ne perd au plus que N pas.
    const int SAVEEVERY = (int)envI("NK_GPT_SAVEEVERY", 0);

    optim::NkAdam adam(params, peakLr, 0.9f, 0.999f, 1e-8f, /*weightDecay=AdamW*/ 0.01f);
    auto saveCkpt = [&](const char* path) -> bool {
        GptMeta meta; meta.V = V; meta.d = (int32)d; meta.H = (int32)H; meta.L = (int32)L; meta.T = (int32)T; meta.langs = langs;
        for (int64 i = 0; i < (int64)bpe.merges.Size(); ++i) meta.merges.PushBack(bpe.merges[(nk_size)i]);
        return SaveCheckpoint(path, meta, params);
    };

    printf("-- Entraînement (%d pas) --\n", STEPS);
    if (ACCUM > 1)
        printf("   Accumulation de gradient : %d micro-lots -> batch effectif = %lld\n",
               ACCUM, (long long)(B * ACCUM));
    printf("   LR schedule : warmup %d pas -> pic %.2e -> cosine (plancher %.0f%%) ; checkpoint tous les %d pas\n",
           WARMUP, (double)peakLr, (double)(minLrRatio * 100), SAVEEVERY);
    double ema = 0;
    NkChrono chrono;
    for (int s = 1; s <= STEPS; ++s) {
        // LR courant : warmup linéaire puis décroissance cosine jusqu'au plancher.
        float lr;
        if (WARMUP > 0 && s <= WARMUP) lr = peakLr * (float)s / (float)WARMUP;
        else {
            const double prog = (STEPS > WARMUP) ? (double)(s - WARMUP) / (double)(STEPS - WARMUP) : 1.0;
            const double cosv = 0.5 * (1.0 + cos(kPi * prog));
            lr = (float)(peakLr * (minLrRatio + (1.0 - minLrRatio) * cosv));
        }
        adam.SetLearningRate(lr);
        adam.ZeroGrad();                 // on ouvre la fenêtre d'accumulation
        double lv = 0.0;
        for (int m = 0; m < ACCUM; ++m) {
            NkTensor x, oneHot; makeBatch(x, oneHot);
            NkVar logits = gpt.Forward(useGpu ? x.ToGPU() : x);
            NkVar loss = autograd::SoftmaxCrossEntropy(logits, NkVar::Leaf(useGpu ? oneHot.ToGPU() : oneHot, false));
            // On divise la loss par ACCUM : les gradients accumulés (AccumGrad = somme)
            // donnent alors la MOYENNE sur les micro-lots, pas la somme.
            NkVar scaled = (ACCUM > 1) ? autograd::MulScalar(loss, 1.0 / (double)ACCUM) : loss;
            scaled.Backward();           // accumule dans params.grad (pas de ZeroGrad ici)
            lv += loss.Value().ToCPU().GetItem(NkShape{ (int64)0 }) / (double)ACCUM;
        }
        adam.Step();                     // un seul pas d'optimiseur par fenêtre
        ema = (s == 1) ? lv : 0.98 * ema + 0.02 * lv;
        if (s % 25 == 0 || s == 1) printf("  pas %4d : perte = %.4f  (moy. %.4f)  lr=%.2e\n", s, lv, ema, (double)lr);
        if (s % 100 == 0) {
            printf("    --- échantillons (pas %d) ---\n", s);
            if (langs.Size() == 0) printf("    %s\n", generate(seed, 100, 0.8, -1).CStr());
            else for (int li = 0; li < (int)langs.Size(); ++li) printf("    [%s] %s\n", langs[(nk_size)li].CStr(), generate(seed, 80, 0.8, li).CStr());
            printf("    ---------------------------\n");
        }
        // Checkpoint périodique (sécurité run long).
        if (SAVEEVERY > 0 && envSave && s % SAVEEVERY == 0) {
            if (saveCkpt(envSave)) printf("  [checkpoint pas %d -> %s]\n", s, envSave);
        }
    }
    printf("Entraînement terminé en %.1f s (%s).\n", chrono.Elapsed().seconds, useGpu ? "GPU-résident" : "CPU");

    // ---- Sauvegarde du modèle (si NK_GPT_SAVE) ----
    if (envSave) {
        GptMeta meta; meta.V = V; meta.d = (int32)d; meta.H = (int32)H; meta.L = (int32)L; meta.T = (int32)T; meta.langs = langs;
        for (int64 i = 0; i < (int64)bpe.merges.Size(); ++i) meta.merges.PushBack(bpe.merges[(nk_size)i]);
        if (SaveCheckpoint(envSave, meta, params)) printf("Modèle sauvegardé : %s\n", envSave);
        else                                       printf("Échec de la sauvegarde : %s\n", envSave);
    }

    // ---- Génération finale (une par langue si multilingue) ----
    printf("\n=== TEXTE GÉNÉRÉ (amorce « %s », %d tokens, temp 0.8) ===\n", seed.CStr(), GENLEN);
    if (langs.Size() <= 1) printf("%s\n", generate(seed, GENLEN, 0.8, langs.Size() == 0 ? -1 : 0).CStr());
    else for (int li = 0; li < (int)langs.Size(); ++li) printf("[%s] %s\n\n", langs[(nk_size)li].CStr(), generate(seed, GENLEN, 0.8, li).CStr());
    printf("=========================================================\n");

    bool ok = ema < 5.0;   // seuil indicatif (vocab BPE => base ln(V) plus élevée)
    printf("\n[%s] le GPT a appris (perte %.2f) et génère du texte %s.\n",
           ok ? " OK " : "FAIL", ema, useGpu ? "100%% sur GPU" : "sur CPU");
    gpu.Shutdown();
    return ok ? 0 : 1;
}
