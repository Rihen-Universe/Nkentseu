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

#include <cstdio>
#include <cstdlib>   // getenv, atol, atoi
#include <math.h>    // exp

using namespace nkentseu;
using namespace nkentseu::ai;

// ---- Lecture d'un fichier entier en NkString (FILE* C, comme NKInfer) ---------
static NkString ReadFileAll(const char* path) {
    FILE* f = fopen(path, "rb");
    NkString s;
    if (!f) return s;
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.Append(buf, (NkString::SizeType)n);
    fclose(f);
    return s;
}

// ---- Corpus : lit un fichier, saute l'entête Gutenberg, cape à maxChars --------
static NkString LoadCorpus(const char* path, nk_size maxChars) {
    NkString all = ReadFileAll(path);
    NkString::SizeType start = 0;
    NkString::SizeType m = all.Find("*** START OF");
    if (m != NkString::npos) {
        NkString::SizeType nl = all.Find("\n", m);
        if (nl != NkString::npos) start = nl + 1;
    }
    NkString::SizeType end = all.Find("*** END OF");
    NkString::SizeType count = (end != NkString::npos && end > start) ? (end - start) : NkString::npos;
    NkString body = all.SubStr(start, count);
    if (body.Size() > (NkString::SizeType)maxChars) body = body.SubStr(0, (NkString::SizeType)maxChars);
    return body;
}

// Langue d'un chemin de fichier = préfixe (avant le premier '_') du nom de fichier.
static NkString LangOf(const NkString& path) {
    const char* p = path.Data(); int64 n = (int64)path.Size();
    int64 base = 0;                                  // début du nom de fichier
    for (int64 i = 0; i < n; ++i) if (p[i] == '/' || p[i] == '\\') base = i + 1;
    for (int64 i = base; i < n; ++i) {
        if (p[i] == '_') { int64 len = i - base; if (len >= 1 && len <= 4) return path.SubStr((NkString::SizeType)base, (NkString::SizeType)len); break; }
    }
    return NkString("??");
}

// ---- Corpus dossier GROUPÉ PAR LANGUE ----------------------------------------
// Remplit `langs` (fr/en/bbj…) et `texts` (parallèle : texte concaténé par langue),
// chaque langue recevant ~totalCap/nbLangues caractères (équilibrage).
static void LoadCorpusByLang(const NkString& dir, nk_size totalCap,
                             NkVector<NkString>& langs, NkVector<NkString>& texts) {
    NkVector<NkString> files = NkDirectory::GetFiles(dir.CStr(), "*.txt",
                                                     NkSearchOption::NK_TOP_DIRECTORY_ONLY);
    // Tri lexicographique (déterminisme) — tri par insertion (peu de fichiers).
    for (int64 i = 1; i < (int64)files.Size(); ++i)
        for (int64 j = i; j > 0; --j) {
            if (!(files[(nk_size)j] < files[(nk_size)(j - 1)])) break;
            NkString tmp = files[(nk_size)j]; files[(nk_size)j] = files[(nk_size)(j - 1)]; files[(nk_size)(j - 1)] = tmp;
        }
    if (files.Size() == 0) return;

    // Regroupe les fichiers par langue (indices dans `files`), ordre stable.
    NkVector<NkVector<int64>> byLang;
    for (int64 fi = 0; fi < (int64)files.Size(); ++fi) {
        NkString lg = LangOf(files[(nk_size)fi]);
        int64 idx = -1;
        for (int64 k = 0; k < (int64)langs.Size(); ++k) if (langs[(nk_size)k] == lg) { idx = k; break; }
        if (idx < 0) { langs.PushBack(lg); byLang.PushBack(NkVector<int64>()); texts.PushBack(NkString()); idx = (int64)langs.Size() - 1; }
        byLang[(nk_size)idx].PushBack(fi);
    }

    const nk_size perLang = totalCap / (nk_size)langs.Size();
    for (int64 li = 0; li < (int64)langs.Size(); ++li) {
        const nk_size perFile = perLang / (nk_size)byLang[(nk_size)li].Size();
        for (int64 bi = 0; bi < (int64)byLang[(nk_size)li].Size(); ++bi) {
            const NkString& path = files[(nk_size)byLang[(nk_size)li][(nk_size)bi]];
            NkString body = LoadCorpus(path.CStr(), perFile);
            if (body.Size() < 200) continue;
            texts[(nk_size)li].Append(body); texts[(nk_size)li].Append("\n\n");
            printf("  + [%-3s] %-32s %8llu car.\n", langs[(nk_size)li].CStr(),
                   path.CStr(), (unsigned long long)body.Size());
        }
        printf("    => langue %-3s : %8llu car. (cible/langue %llu)\n",
               langs[(nk_size)li].CStr(), (unsigned long long)texts[(nk_size)li].Size(), (unsigned long long)perLang);
    }
}

// ================= Table de hachage int64->int64 (open addressing) ============
// Zéro-STL (sur NkVector). Sert au comptage de paires BPE (Add = increment) et au
// rang des fusions (Get). Suit le max courant pour un argmax O(1).
static const int64 kEmpty = (int64)0x8000000000000000LL;
struct I64Map {
    NkVector<int64> keys;
    NkVector<int64> vals;
    int64 mask = 0;
    int64 bestKey = -1, bestVal = 0;
    void Init(int64 pow2) {
        keys.Clear(); vals.Clear();
        keys.Reserve((nk_size)pow2); vals.Reserve((nk_size)pow2);
        for (int64 i = 0; i < pow2; ++i) { keys.PushBack(kEmpty); vals.PushBack(0); }
        mask = pow2 - 1; bestKey = -1; bestVal = 0;
    }
    void Reset() {
        for (int64 i = 0; i <= mask; ++i) { keys[(nk_size)i] = kEmpty; vals[(nk_size)i] = 0; }
        bestKey = -1; bestVal = 0;
    }
    static uint64 Hash(int64 k) { uint64 h = (uint64)k * 1099511628211ULL; h ^= h >> 29; h *= 1099511628211ULL; h ^= h >> 32; return h; }
    void Add(int64 k, int64 w) {
        int64 s = (int64)(Hash(k) & (uint64)mask);
        while (keys[(nk_size)s] != kEmpty && keys[(nk_size)s] != k) s = (s + 1) & mask;
        if (keys[(nk_size)s] == kEmpty) { keys[(nk_size)s] = k; vals[(nk_size)s] = w; }
        else vals[(nk_size)s] += w;
        int64 nv = vals[(nk_size)s];
        if (nv > bestVal) { bestVal = nv; bestKey = k; }
    }
    int64 Get(int64 k, int64 def) const {
        int64 s = (int64)(Hash(k) & (uint64)mask);
        while (keys[(nk_size)s] != kEmpty) { if (keys[(nk_size)s] == k) return vals[(nk_size)s]; s = (s + 1) & mask; }
        return def;
    }
};

static int64 PairKey(int32 a, int32 b) { return ((int64)a << 21) | (int64)b; }

// ================= BPE (Byte-Pair Encoding) from-scratch ======================
struct NkMerge { int32 a = 0, b = 0; };
struct Bpe {
    NkVector<NkMerge> merges;
    NkVector<NkString> vocab;     // id -> octets (décodage)
    I64Map rank;                  // (a,b) -> priorité de fusion
    int Base() const { return 256 + (int)merges.Size(); }

    void BuildVocabRank() {
        vocab.Clear();
        for (int b = 0; b < 256; ++b) { NkString s; s.Append((char)b); vocab.PushBack(s); }
        int64 cap = 2; while (cap < (int64)(merges.Size() * 2 + 8)) cap <<= 1;
        rank.Init(cap);
        for (int64 i = 0; i < (int64)merges.Size(); ++i) {
            NkString t = vocab[(nk_size)merges[(nk_size)i].a]; t.Append(vocab[(nk_size)merges[(nk_size)i].b]);
            vocab.PushBack(t);
            rank.Add(PairKey(merges[(nk_size)i].a, merges[(nk_size)i].b), i);
        }
    }

    static void PreTok(const NkString& text, NkVector<NkString>& words) {
        const char* p = text.Data(); int64 n = (int64)text.Size();
        NkString cur;
        for (int64 i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)p[i];
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') { if (cur.Size() > 0) words.PushBack(cur); cur = NkString(); cur.Append((char)c); }
            else cur.Append((char)c);
        }
        if (cur.Size() > 0) words.PushBack(cur);
    }

    void EncodeWord(const NkString& w, NkVector<int32>& out) const {
        NkVector<int32> seq;
        const char* p = w.Data(); int64 n = (int64)w.Size();
        for (int64 i = 0; i < n; ++i) seq.PushBack((int32)(unsigned char)p[i]);
        while (seq.Size() >= 2) {
            int64 bestRank = 0x7fffffffLL; int64 bestPos = -1;
            for (int64 i = 0; i + 1 < (int64)seq.Size(); ++i) {
                int64 r = rank.Get(PairKey(seq[(nk_size)i], seq[(nk_size)(i + 1)]), 0x7fffffffLL);
                if (r < bestRank) { bestRank = r; bestPos = i; }
            }
            if (bestPos < 0) break;
            seq[(nk_size)bestPos] = 256 + (int32)bestRank;
            for (int64 j = bestPos + 1; j + 1 < (int64)seq.Size(); ++j) seq[(nk_size)j] = seq[(nk_size)(j + 1)];
            seq.Resize((nk_size)(seq.Size() - 1));
        }
        for (int64 i = 0; i < (int64)seq.Size(); ++i) out.PushBack(seq[(nk_size)i]);
    }
    void Encode(const NkString& text, NkVector<int32>& out) const {
        NkVector<NkString> words; PreTok(text, words);
        for (int64 i = 0; i < (int64)words.Size(); ++i) EncodeWord(words[(nk_size)i], out);
    }
    const NkString& Decode(int id) const { return vocab[(nk_size)id]; }
};

// Entraîne le BPE : fusionne itérativement la paire adjacente la plus fréquente,
// sur un tableau plat d'octets (mots séparés par SEP=-1). Table de hachage maison.
static void TrainBpe(const NkVector<NkString>& texts, int nMerges, Bpe& bpe) {
    const int32 SEP = -1;
    const int64 CAP = 800000;                       // borne le corpus d'apprentissage BPE
    NkVector<int32> flat;
    for (int64 ti = 0; ti < (int64)texts.Size() && (int64)flat.Size() < CAP; ++ti) {
        NkVector<NkString> words; Bpe::PreTok(texts[(nk_size)ti], words);
        for (int64 wi = 0; wi < (int64)words.Size(); ++wi) {
            const NkString& w = words[(nk_size)wi];
            const char* p = w.Data(); int64 n = (int64)w.Size();
            for (int64 i = 0; i < n; ++i) flat.PushBack((int32)(unsigned char)p[i]);
            flat.PushBack(SEP);
            if ((int64)flat.Size() >= CAP) break;
        }
    }
    I64Map pc; int64 cap = 1; while (cap < (1 << 19)) cap <<= 1; pc.Init(cap);
    for (int m = 0; m < nMerges; ++m) {
        pc.Reset();
        int64 N = (int64)flat.Size();
        for (int64 i = 0; i + 1 < N; ++i) { int32 a = flat[(nk_size)i], b = flat[(nk_size)(i + 1)]; if (a == SEP || b == SEP) continue; pc.Add(PairKey(a, b), 1); }
        if (pc.bestKey < 0 || pc.bestVal < 2) break;
        int32 a = (int32)(pc.bestKey >> 21), b = (int32)(pc.bestKey & ((1 << 21) - 1));
        NkMerge mg; mg.a = a; mg.b = b; bpe.merges.PushBack(mg);
        int32 newId = 256 + (int32)(bpe.merges.Size() - 1);
        int64 w = 0;
        for (int64 r = 0; r < N; ) {
            if (r + 1 < N && flat[(nk_size)r] == a && flat[(nk_size)(r + 1)] == b) { flat[(nk_size)w] = newId; ++w; r += 2; }
            else { flat[(nk_size)w] = flat[(nk_size)r]; ++w; r += 1; }
        }
        flat.Resize((nk_size)w);
        if ((m + 1) % 200 == 0) printf("  BPE : %d/%d fusions...\n", m + 1, nMerges);
    }
    bpe.BuildVocabRank();
}

// ---- Checkpoint « NKGP » v3 : dims + BPE (fusions) + langues + poids (CPU) ----
struct GptMeta { int32 V = 0, d = 0, H = 0, L = 0, T = 0; NkVector<NkMerge> merges; NkVector<NkString> langs; };

static int64 ShapeNumel(const NkShape& sh) { int64 n = 1; for (uint32 i = 0; i < sh.Size(); ++i) n *= sh[i]; return n; }

static bool SaveCheckpoint(const char* path, const GptMeta& m, const NkVector<NkVar>& params) {
    FILE* f = fopen(path, "wb"); if (!f) return false;
    const char magic[4] = { 'N','K','G','P' }; uint32 ver = 3u;
    bool ok = fwrite(magic, 1, 4, f) == 4 && fwrite(&ver, sizeof(uint32), 1, f) == 1;
    int32 hdr[5] = { m.V, m.d, m.H, m.L, m.T };
    ok = ok && fwrite(hdr, sizeof(int32), 5, f) == 5;
    int32 nMerges = (int32)m.merges.Size();
    ok = ok && fwrite(&nMerges, sizeof(int32), 1, f) == 1;
    for (int32 i = 0; ok && i < nMerges; ++i) { int32 ab[2] = { m.merges[(nk_size)i].a, m.merges[(nk_size)i].b }; ok = fwrite(ab, sizeof(int32), 2, f) == 2; }
    int32 nLang = (int32)m.langs.Size();
    ok = ok && fwrite(&nLang, sizeof(int32), 1, f) == 1;
    for (int32 i = 0; ok && i < nLang; ++i) {
        uint8 ln = (uint8)m.langs[(nk_size)i].Size();
        ok = ok && fwrite(&ln, 1, 1, f) == 1 && (ln == 0 || fwrite(m.langs[(nk_size)i].CStr(), 1, ln, f) == ln);
    }
    uint32 count = params.Size();
    ok = ok && fwrite(&count, sizeof(uint32), 1, f) == 1;
    for (uint32 i = 0; ok && i < params.Size(); ++i) {
        NkTensor v = params[i].Value().ToCPU().Contiguous();
        const NkShape& sh = v.Shape(); uint32 rank = sh.Size();
        ok = ok && fwrite(&rank, sizeof(uint32), 1, f) == 1;
        for (uint32 dd = 0; ok && dd < rank; ++dd) { int64 dim = sh[dd]; ok = ok && fwrite(&dim, sizeof(int64), 1, f) == 1; }
        int64 numel = ShapeNumel(sh); const float* p = v.DataAs<float>();
        ok = ok && (numel == 0 || fwrite(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
    }
    fclose(f); return ok;
}

static bool LoadCheckpointMeta(const char* path, GptMeta& m) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    char magic[4]; uint32 ver = 0; int32 hdr[5] = { 0 };
    bool ok = fread(magic, 1, 4, f) == 4 && magic[0]=='N'&&magic[1]=='K'&&magic[2]=='G'&&magic[3]=='P'
           && fread(&ver, sizeof(uint32), 1, f) == 1 && ver == 3u
           && fread(hdr, sizeof(int32), 5, f) == 5;
    if (ok) { m.V = hdr[0]; m.d = hdr[1]; m.H = hdr[2]; m.L = hdr[3]; m.T = hdr[4]; }
    if (ok) {
        int32 nMerges = 0; ok = fread(&nMerges, sizeof(int32), 1, f) == 1 && nMerges >= 0 && nMerges <= 200000;
        for (int32 i = 0; ok && i < nMerges; ++i) { int32 ab[2]; ok = fread(ab, sizeof(int32), 2, f) == 2; if (ok) { NkMerge mg; mg.a = ab[0]; mg.b = ab[1]; m.merges.PushBack(mg); } }
    }
    if (ok) {
        int32 nLang = 0; ok = fread(&nLang, sizeof(int32), 1, f) == 1 && nLang >= 0 && nLang <= 64;
        for (int32 i = 0; ok && i < nLang; ++i) {
            uint8 ln = 0; char buf[256];
            ok = fread(&ln, 1, 1, f) == 1 && (ln == 0 || fread(buf, 1, ln, f) == ln);
            if (ok) m.langs.PushBack(NkString(buf, (NkString::SizeType)ln));
        }
    }
    fclose(f); return ok;
}

static bool LoadCheckpointWeights(const char* path, NkVector<NkVar>& params) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    char magic[4]; uint32 ver = 0; int32 hdr[5] = { 0 };
    bool ok = fread(magic, 1, 4, f) == 4 && fread(&ver, sizeof(uint32), 1, f) == 1
           && fread(hdr, sizeof(int32), 5, f) == 5;
    if (ok) { int32 nMerges = 0; ok = fread(&nMerges, sizeof(int32), 1, f) == 1 && nMerges >= 0; if (ok && nMerges > 0) ok = fseek(f, (long)nMerges * 2 * (long)sizeof(int32), SEEK_CUR) == 0; }
    if (ok) {
        int32 nLang = 0; ok = fread(&nLang, sizeof(int32), 1, f) == 1 && nLang >= 0 && nLang <= 64;
        for (int32 i = 0; ok && i < nLang; ++i) { uint8 ln = 0; ok = fread(&ln, 1, 1, f) == 1 && (ln == 0 || fseek(f, (long)ln, SEEK_CUR) == 0); }
    }
    uint32 count = 0;
    ok = ok && fread(&count, sizeof(uint32), 1, f) == 1 && count == params.Size();
    for (uint32 i = 0; ok && i < params.Size(); ++i) {
        uint32 rank = 0; ok = ok && fread(&rank, sizeof(uint32), 1, f) == 1;
        NkShape shape;
        for (uint32 dd = 0; ok && dd < rank; ++dd) { int64 dim = 0; ok = ok && fread(&dim, sizeof(int64), 1, f) == 1; shape.PushBack(dim); }
        int64 numel = ok ? ShapeNumel(shape) : 0;
        NkTensor t = NkTensor::Zeros(shape); float* p = t.DataAs<float>();
        ok = ok && (numel == 0 || fread(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
        if (ok) params[i].SetValue(t);
    }
    fclose(f); return ok;
}

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
        langData.Resize((nk_size)texts.Size());
        langMask.Resize((nk_size)texts.Size());
        nk_size totalTok = 0;
        const NkStringView marker("Réponse: ");
        for (int64 li = 0; li < (int64)texts.Size(); ++li) {
            const NkString& txt = texts[(nk_size)li];
            // Masquage de loss : un tag contenant des blocs "Question:/Réponse:" est traité
            // en instruction-tuning — la QUESTION est masquée (loss=0), seule la RÉPONSE compte.
            const bool isQa = txt.Find(marker) != NkString::npos;
            if (!isQa) {
                NkVector<int32> ids; bpe.Encode(txt, ids);
                for (int64 k = 0; k < (int64)ids.Size(); ++k) {
                    langData[(nk_size)li].PushBack((float)ids[(nk_size)k]);
                    langMask[(nk_size)li].PushBack(1.f);   // prose/code : tout compte (comportement d'origine)
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
                    // Partie question (jusqu'à "Réponse: " inclus) -> masque 0.
                    NkString qPart = (mp == NkString::npos) ? block : block.SubStr(0, mp + marker.Size());
                    NkVector<int32> qIds; bpe.Encode(qPart, qIds);
                    for (int64 k = 0; k < (int64)qIds.Size(); ++k) { langData[(nk_size)li].PushBack((float)qIds[(nk_size)k]); langMask[(nk_size)li].PushBack(0.f); }
                    // Partie réponse -> masque 1 (seule à compter dans la loss).
                    if (mp != NkString::npos) {
                        NkString aPart = block.SubStr(mp + marker.Size());
                        if (aPart.Size() > 0) {
                            NkVector<int32> aIds; bpe.Encode(aPart, aIds);
                            for (int64 k = 0; k < (int64)aIds.Size(); ++k) { langData[(nk_size)li].PushBack((float)aIds[(nk_size)k]); langMask[(nk_size)li].PushBack(1.f); }
                        }
                    }
                    // Séparateur \n\n entre blocs -> masqué.
                    NkVector<int32> sepIds; bpe.Encode(NkString("\n\n"), sepIds);
                    for (int64 k = 0; k < (int64)sepIds.Size(); ++k) { langData[(nk_size)li].PushBack((float)sepIds[(nk_size)k]); langMask[(nk_size)li].PushBack(0.f); }
                }
            }
            totalTok += langData[(nk_size)li].Size();
        }
        printf("Corpus : %llu car. -> %llu tokens BPE ; %d tokens (256 + %llu fusions) + %d tags = vocab %d.\n",
               (unsigned long long)totalChars, (unsigned long long)totalTok, nByte, (unsigned long long)bpe.merges.Size(), (int)langs.Size(), V);
        T = envI("NK_GPT_T", 128); d = envI("NK_GPT_D", 256);
        H = envI("NK_GPT_H", 8);   L = envI("NK_GPT_L", 4);
        printf("Modèle GPT : T=%lld, d=%lld, têtes=%lld, couches=%lld, batch=%lld  (AdamW, GPU-résident)\n\n",
               (long long)T,(long long)d,(long long)H,(long long)L,(long long)B);
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

    // ---- Mode CHARGEMENT : on génère et on sort (aucun entraînement) ----
    if (envLoad) {
        printf("\n=== TEXTE GÉNÉRÉ (langue %s, amorce « %s », %d tokens) ===\n",
               genLang >= 0 ? langs[(nk_size)genLang].CStr() : "auto", seed.CStr(), GENLEN);
        printf("%s\n", generate(seed, GENLEN, 0.8, genLang).CStr());
        printf("=========================================================\n");
        gpu.Shutdown();
        return 0;
    }

    // ---- Entraînement ----
    optim::NkAdam adam(params, 3e-4f, 0.9f, 0.999f, 1e-8f, /*weightDecay=AdamW*/ 0.01f);
    const char* envs = getenv("NK_GPT_STEPS");
    const int STEPS = envs ? atoi(envs) : 300;
    // Accumulation de gradient : ACCUM micro-lots -> batch EFFECTIF = B*ACCUM, avec la
    // mémoire d'activations d'UN SEUL micro-lot (B). Levier n°1 pour tenir un gros modèle
    // sur une VRAM limitée sans réduire la qualité de gradient. NK_GPT_ACCUM (défaut 1).
    const int ACCUM = (int)envI("NK_GPT_ACCUM", 1);
    printf("-- Entraînement (%d pas) --\n", STEPS);
    if (ACCUM > 1)
        printf("   Accumulation de gradient : %d micro-lots -> batch effectif = %lld\n",
               ACCUM, (long long)(B * ACCUM));
    double ema = 0;
    NkChrono chrono;
    for (int s = 1; s <= STEPS; ++s) {
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
        if (s % 25 == 0 || s == 1) printf("  pas %4d : perte = %.4f  (moy. %.4f)\n", s, lv, ema);
        if (s % 100 == 0) {
            printf("    --- échantillons (pas %d) ---\n", s);
            if (langs.Size() == 0) printf("    %s\n", generate(seed, 100, 0.8, -1).CStr());
            else for (int li = 0; li < (int)langs.Size(); ++li) printf("    [%s] %s\n", langs[(nk_size)li].CStr(), generate(seed, 80, 0.8, li).CStr());
            printf("    ---------------------------\n");
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
