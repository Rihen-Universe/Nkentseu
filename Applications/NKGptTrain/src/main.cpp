// =============================================================================
// NKGptTrain — petit GPT char-level, from-scratch, entraîné 100% GPU-résident,
// qui GÉNÈRE du texte. Assemble tout NKAI : NkGPT (NKNN) + AdamW (NKOptim) +
// softmax-CE (NKAutograd), sur un corpus texte réel (Project Gutenberg).
//
//   Tokenizer : BPE from-scratch (256 octets + fusions ; NK_GPT_MERGES, défaut 600)
//               → le modèle manipule des morceaux de mots (texte plus lisible).
//   Entraînement : boucle + échantillonnage autoregressif, tag de langue.
//
// Corpus  : NK_GPT_DIR (défaut = tout Datasets) ou NK_GPT_FILE (un livre).
// Taille  : NK_GPT_T/D/H/L/B, NK_GPT_MERGES ; étapes : NK_GPT_STEPS (défaut 300).
// Modèle  : NK_GPT_SAVE=chemin (sauve après entraînement),
//           NK_GPT_LOAD=chemin (recharge + génère SANS réentraîner),
//           NK_GPT_PROMPT="amorce", NK_GPT_GENLEN=nb car. générés,
//           NK_GPT_LANG=fr|en|bbj (pilote la langue via le tag de langue).
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
#include <climits>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <map>
#include <utility>

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

// Langue d'un fichier = préfixe avant le premier '_' (ex. "fr_pg17989.txt" -> "fr").
static std::string LangOf(const std::string& filename) {
    size_t u = filename.find('_');
    if (u != std::string::npos && u >= 1 && u <= 4) return filename.substr(0, u);
    return "??";   // fichier non taggé
}

// ---- Corpus dossier GROUPÉ PAR LANGUE ----------------------------------------
// Remplit `langs` (noms, ex. fr/en/bbj) et `texts` (parallèle : texte concaténé
// de chaque langue). Chaque langue reçoit ~totalCap/nbLangues caractères, répartis
// également entre ses fichiers. Le tag de langue permet ensuite de piloter la
// langue générée.
static void LoadCorpusByLang(const std::string& dir, size_t totalCap,
                             std::vector<std::string>& langs, std::vector<std::string>& texts) {
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), endIt; !ec && it != endIt; it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        std::string p = it->path().string();
        if (p.size() >= 4 && p.compare(p.size() - 4, 4, ".txt") == 0) files.push_back(p);
    }
    std::sort(files.begin(), files.end());               // déterministe
    if (files.empty()) return;

    // Regroupe les fichiers par langue (ordre d'apparition stable).
    std::vector<std::vector<std::string>> byLang;
    for (const std::string& p : files) {
        std::string lg = LangOf(fs::path(p).filename().string());
        int idx = -1;
        for (int i = 0; i < (int)langs.size(); ++i) if (langs[i] == lg) { idx = i; break; }
        if (idx < 0) { langs.push_back(lg); byLang.push_back(std::vector<std::string>()); texts.push_back(std::string()); idx = (int)langs.size() - 1; }
        byLang[idx].push_back(p);
    }

    const size_t perLang = totalCap / langs.size();       // part égale PAR LANGUE
    for (size_t li = 0; li < langs.size(); ++li) {
        const size_t perFile = perLang / byLang[li].size();
        for (const std::string& p : byLang[li]) {
            std::string body = LoadCorpus(p, perFile);
            if (body.size() < 200) continue;             // ignore fichiers vides/parasites
            texts[li] += body; texts[li] += "\n\n";
            printf("  + [%-3s] %-32s %8zu car.\n", langs[li].c_str(),
                   fs::path(p).filename().string().c_str(), body.size());
        }
        printf("    => langue %-3s : %8zu car. (cible/langue %zu)\n", langs[li].c_str(), texts[li].size(), perLang);
    }
}

// ================= BPE (Byte-Pair Encoding) from-scratch ======================
// Base : 256 tokens octets (id = valeur d'octet, gère tout l'UTF-8). Puis nMerges
// FUSIONS : le token 256+i provient de la paire merges[i]. Le vocab (id -> octets)
// est reconstruit par concaténation. Le modèle manipule ainsi des MORCEAUX DE MOTS
// au lieu de lettres → texte bien plus lisible.
struct Bpe {
    std::vector<std::pair<int,int>> merges;           // fusions ordonnées
    std::vector<std::string> vocab;                   // id -> octets (décodage)
    std::map<std::pair<int,int>,int> rank;            // (a,b) -> priorité (index de fusion)

    int Base() const { return 256 + (int)merges.size(); }   // nb de tokens réels (hors tags)

    void BuildVocabRank() {
        vocab.assign(256, std::string());
        for (int b = 0; b < 256; ++b) vocab[b] = std::string(1, (char)b);
        rank.clear();
        for (int i = 0; i < (int)merges.size(); ++i) {
            vocab.push_back(vocab[merges[i].first] + vocab[merges[i].second]);
            rank[merges[i]] = i;
        }
    }

    // Pré-tokenisation façon GPT-2 : un blanc démarre un nouveau « mot » (le blanc
    // reste attaché au mot suivant → " the " devient un token).
    static std::vector<std::string> PreTok(const std::string& text) {
        std::vector<std::string> words; std::string cur;
        for (unsigned char c : text) {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
                if (!cur.empty()) words.push_back(cur);
                cur = std::string(1, (char)c);
            } else cur += (char)c;
        }
        if (!cur.empty()) words.push_back(cur);
        return words;
    }

    // Encode un mot (octets) en ids, en appliquant les fusions par ordre de priorité.
    void EncodeWord(const std::string& w, std::vector<int>& out) const {
        std::vector<int> seq; seq.reserve(w.size());
        for (unsigned char c : w) seq.push_back((int)c);
        while (seq.size() >= 2) {
            int bestRank = INT_MAX, bestPos = -1;
            for (size_t i = 0; i + 1 < seq.size(); ++i) {
                auto it = rank.find(std::make_pair(seq[i], seq[i + 1]));
                if (it != rank.end() && it->second < bestRank) { bestRank = it->second; bestPos = (int)i; }
            }
            if (bestPos < 0) break;
            seq[bestPos] = 256 + bestRank;
            seq.erase(seq.begin() + bestPos + 1);
        }
        for (int id : seq) out.push_back(id);
    }

    void Encode(const std::string& text, std::vector<int>& out) const {
        for (const std::string& w : PreTok(text)) EncodeWord(w, out);
    }
    std::string Decode(int id) const { return (id >= 0 && id < (int)vocab.size()) ? vocab[id] : std::string(); }
};

// Entraîne le BPE : fusionne itérativement la paire adjacente la plus fréquente,
// sur les MOTS UNIQUES pondérés par leur fréquence (BPE classique, efficace).
static void TrainBpe(const std::vector<std::string>& texts, int nMerges, Bpe& bpe) {
    std::map<std::string, long> wf;
    for (const std::string& t : texts) for (const std::string& w : Bpe::PreTok(t)) wf[w]++;
    std::vector<std::vector<int>> seqs; std::vector<long> freq;
    seqs.reserve(wf.size()); freq.reserve(wf.size());
    for (const auto& kv : wf) { std::vector<int> s; s.reserve(kv.first.size()); for (unsigned char c : kv.first) s.push_back((int)c); seqs.push_back(std::move(s)); freq.push_back(kv.second); }
    for (int m = 0; m < nMerges; ++m) {
        std::map<std::pair<int,int>, long> pc;
        for (size_t k = 0; k < seqs.size(); ++k) { const std::vector<int>& s = seqs[k]; for (size_t i = 0; i + 1 < s.size(); ++i) pc[std::make_pair(s[i], s[i + 1])] += freq[k]; }
        if (pc.empty()) break;
        std::pair<int,int> best; long bestC = 0;
        for (const auto& kv : pc) if (kv.second > bestC) { bestC = kv.second; best = kv.first; }
        if (bestC < 2) break;                       // plus rien de fréquent à fusionner
        const int newId = 256 + (int)bpe.merges.size();
        bpe.merges.push_back(best);
        for (std::vector<int>& s : seqs)
            for (size_t i = 0; i + 1 < s.size(); ) {
                if (s[i] == best.first && s[i + 1] == best.second) { s[i] = newId; s.erase(s.begin() + i + 1); }
                else ++i;
            }
        if ((m + 1) % 200 == 0) printf("  BPE : %d/%d fusions...\n", m + 1, nMerges);
    }
    bpe.BuildVocabRank();
}

// ---- Checkpoint « NKGP » v3 : dims + BPE (fusions) + langues + poids (CPU) ----
// Format : ['N','K','G','P'] | ver=3 u32 | V,d,H,L,T (5×i32) | nMerges i32 |
//   merges[nMerges] (2×i32) | nLang i32 | {len u8, octets} par langue |
//   count u32 | { rank u32, dims[rank] i64, data[numel] f32 } par tenseur.
struct GptMeta { int32 V = 0, d = 0, H = 0, L = 0, T = 0; std::vector<std::pair<int32,int32>> merges; std::vector<std::string> langs; };

static int64 ShapeNumel(const NkShape& sh) { int64 n = 1; for (uint32 i = 0; i < sh.Size(); ++i) n *= sh[i]; return n; }

static bool SaveCheckpoint(const char* path, const GptMeta& m, const NkVector<NkVar>& params) {
    FILE* f = fopen(path, "wb"); if (!f) return false;
    const char magic[4] = { 'N','K','G','P' }; uint32 ver = 3u;   // v3 : tokenizer BPE
    bool ok = fwrite(magic, 1, 4, f) == 4 && fwrite(&ver, sizeof(uint32), 1, f) == 1;
    int32 hdr[5] = { m.V, m.d, m.H, m.L, m.T };
    ok = ok && fwrite(hdr, sizeof(int32), 5, f) == 5;
    // BPE : nMerges puis les paires (a,b).
    int32 nMerges = (int32)m.merges.size();
    ok = ok && fwrite(&nMerges, sizeof(int32), 1, f) == 1;
    for (int32 i = 0; ok && i < nMerges; ++i) {
        int32 ab[2] = { m.merges[i].first, m.merges[i].second };
        ok = fwrite(ab, sizeof(int32), 2, f) == 2;
    }
    // Langues (tag de langue) : nLang, puis pour chacune len(u8)+octets.
    int32 nLang = (int32)m.langs.size();
    ok = ok && fwrite(&nLang, sizeof(int32), 1, f) == 1;
    for (int32 i = 0; ok && i < nLang; ++i) {
        uint8 ln = (uint8)m.langs[i].size();
        ok = ok && fwrite(&ln, 1, 1, f) == 1 && (ln == 0 || fwrite(m.langs[i].data(), 1, ln, f) == ln);
    }
    uint32 count = params.Size();
    ok = ok && fwrite(&count, sizeof(uint32), 1, f) == 1;
    for (uint32 i = 0; ok && i < params.Size(); ++i) {
        NkTensor v = params[i].Value().ToCPU().Contiguous();   // ramène GPU->CPU si besoin
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
    if (ok) {   // fusions BPE
        int32 nMerges = 0; ok = fread(&nMerges, sizeof(int32), 1, f) == 1 && nMerges >= 0 && nMerges <= 200000;
        for (int32 i = 0; ok && i < nMerges; ++i) { int32 ab[2]; ok = fread(ab, sizeof(int32), 2, f) == 2; if (ok) m.merges.push_back(std::make_pair(ab[0], ab[1])); }
    }
    if (ok) {   // langues (tag de langue)
        int32 nLang = 0; ok = fread(&nLang, sizeof(int32), 1, f) == 1 && nLang >= 0 && nLang <= 64;
        for (int32 i = 0; ok && i < nLang; ++i) {
            uint8 ln = 0; char buf[256];
            ok = fread(&ln, 1, 1, f) == 1 && (ln == 0 || fread(buf, 1, ln, f) == ln);
            if (ok) m.langs.push_back(std::string(buf, ln));
        }
    }
    fclose(f); return ok;
}

static bool LoadCheckpointWeights(const char* path, NkVector<NkVar>& params) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    char magic[4]; uint32 ver = 0; int32 hdr[5] = { 0 };
    bool ok = fread(magic, 1, 4, f) == 4 && fread(&ver, sizeof(uint32), 1, f) == 1
           && fread(hdr, sizeof(int32), 5, f) == 5;
    if (ok) {   // saute les fusions BPE
        int32 nMerges = 0; ok = fread(&nMerges, sizeof(int32), 1, f) == 1 && nMerges >= 0;
        if (ok && nMerges > 0) ok = fseek(f, (long)nMerges * 2 * (long)sizeof(int32), SEEK_CUR) == 0;
    }
    if (ok) {   // saute la section langues
        int32 nLang = 0; ok = fread(&nLang, sizeof(int32), 1, f) == 1 && nLang >= 0 && nLang <= 64;
        for (int32 i = 0; ok && i < nLang; ++i) {
            uint8 ln = 0; ok = fread(&ln, 1, 1, f) == 1 && (ln == 0 || fseek(f, (long)ln, SEEK_CUR) == 0);
        }
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
    printf("=== NKGptTrain : petit GPT BPE (from-scratch, GPU-résident) ===\n");
    NkTensorGpu& gpu = NkTensorGpu::Get();
    const bool useGpu = gpu.IsAvailable();
    printf("GPU compute : %s (%s)\n", useGpu ? "OUI" : "NON", gpu.BackendName());

    auto envI = [](const char* k, int64 def) -> int64 { const char* v = getenv(k); return v ? (int64)atol(v) : def; };
    const char* envLoad   = getenv("NK_GPT_LOAD");    // charge un checkpoint -> génère sans réentraîner
    const char* envSave   = getenv("NK_GPT_SAVE");    // sauve le modèle à la fin de l'entraînement
    const char* envPrompt = getenv("NK_GPT_PROMPT");  // amorce de génération (défaut « Le »)
    const std::string seed = envPrompt ? std::string(envPrompt) : std::string("Le ");

    // Tokenizer BPE + LANGUES (tag) + données corpus (train seulement).
    // Tokens réels = nByte (256 octets + fusions BPE). V = nByte + 1 tag par langue.
    Bpe bpe;
    std::vector<std::string> langs;                  // noms de langues (fr/en/bbj…)
    std::vector<std::vector<float>> langData;        // ids BPE par langue (train)
    int V = 0, nByte = 0;
    int64 T = 0, d = 0, H = 0, L = 0, B = envI("NK_GPT_B", 16);

    if (envLoad) {
        // ---- Mode CHARGEMENT : dims + BPE + langues depuis le checkpoint ----
        GptMeta meta;
        if (!LoadCheckpointMeta(envLoad, meta)) { printf("Checkpoint illisible ou format obsolète (attendu BPE v3) : %s\n", envLoad); return 2; }
        V = meta.V; d = meta.d; H = meta.H; L = meta.L; T = meta.T;
        langs = meta.langs;
        for (auto& ab : meta.merges) bpe.merges.push_back(std::make_pair((int)ab.first, (int)ab.second));
        bpe.BuildVocabRank();
        nByte = bpe.Base();
        printf("Modèle chargé : %s (V=%d, T=%lld, d=%lld, têtes=%lld, couches=%lld, %zu fusions BPE)\n",
               envLoad, V, (long long)T, (long long)d, (long long)H, (long long)L, bpe.merges.size());
        if (!langs.empty()) { printf("Langues (NK_GPT_LANG) :"); for (auto& lg : langs) printf(" %s", lg.c_str()); printf("\n"); }
    } else {
        // ---- Corpus ----
        // Défaut : TOUT le dossier Datasets (équilibré par langue). NK_GPT_FILE force
        // un seul livre ; NK_GPT_DIR change le dossier ; NK_GPT_CHARS = cap total.
        const char* envf = getenv("NK_GPT_FILE");
        const char* envd = getenv("NK_GPT_DIR");
        const char* envc = getenv("NK_GPT_CHARS");
        const std::string datasetsDir = envd ? envd
            : "D:/Projets/2026/Nkentseu/Nkentseu/Resources/Datasets";
        std::vector<std::string> texts;
        if (envf) {
            size_t maxChars = envc ? (size_t)atol(envc) : 150000;
            printf("Corpus : fichier unique %s\n", envf);
            langs.push_back(LangOf(std::filesystem::path(envf).filename().string()));
            texts.push_back(LoadCorpus(envf, maxChars));
        } else {
            size_t totalCap = envc ? (size_t)atol(envc) : 1200000;   // ~1,2 M car. par défaut
            printf("Corpus : dossier %s (équilibré par langue, cap total %zu)\n", datasetsDir.c_str(), totalCap);
            LoadCorpusByLang(datasetsDir, totalCap, langs, texts);
        }
        size_t totalChars = 0; for (auto& t : texts) totalChars += t.size();
        if (totalChars < 1000) { printf("Corpus introuvable/trop court.\n"); return 2; }
        // ---- Entraînement du tokenizer BPE (fusions), réglable via NK_GPT_MERGES ----
        const int nMerges = (int)envI("NK_GPT_MERGES", 600);
        printf("Entraînement du tokenizer BPE (%d fusions cible)...\n", nMerges);
        TrainBpe(texts, nMerges, bpe);
        nByte = bpe.Base();
        V = nByte + (int)langs.size();               // + un token-tag par langue
        // Encode chaque langue en ids BPE.
        langData.resize(texts.size());
        size_t totalTok = 0;
        for (size_t li = 0; li < texts.size(); ++li) {
            std::vector<int> ids; bpe.Encode(texts[li], ids);
            langData[li].reserve(ids.size());
            for (int id : ids) langData[li].push_back((float)id);
            totalTok += ids.size();
        }
        printf("Corpus : %zu car. -> %zu tokens BPE ; %d tokens (256 + %zu fusions) + %d tags = vocab %d.\n",
               totalChars, totalTok, nByte, bpe.merges.size(), (int)langs.size(), V);
        // ---- Dimensions modèle (réglables : NK_GPT_D/H/L/T) ----
        T = envI("NK_GPT_T", 128); d = envI("NK_GPT_D", 256);
        H = envI("NK_GPT_H", 8);   L = envI("NK_GPT_L", 4);
        printf("Modèle GPT : T=%lld, d=%lld, têtes=%lld, couches=%lld, batch=%lld  (AdamW, GPU-résident)\n\n",
               (long long)T,(long long)d,(long long)H,(long long)L,(long long)B);
    }
    // Langue de génération demandée (NK_GPT_LANG=fr/en/bbj) — -1 = auto (pas de tag).
    const char* envLang = getenv("NK_GPT_LANG");
    int genLang = -1;
    if (envLang) for (int i = 0; i < (int)langs.size(); ++i) if (langs[i] == envLang) { genLang = i; break; }

    // ---- Construction + (chargement des poids | init aléatoire) ----
    nn::NkGPT gpt((uint32)V, (uint32)d, (uint32)H, (uint32)L, (uint32)T, 1234u);
    NkVector<NkVar> params; gpt.Parameters(params);
    if (envLoad) {
        if (!LoadCheckpointWeights(envLoad, params)) { printf("Poids du checkpoint incompatibles avec les dims.\n"); return 2; }
        printf("Poids rechargés (%u tenseurs).\n", params.Size());
    }
    if (useGpu) for (uint32 i = 0; i < params.Size(); ++i) params[i].SetValue(params[i].Value().ToGPU());

    // RNG déterministe (LCG) pour échantillonner batches et génération.
    uint64 rng = 0x9E3779B97F4A7C15ull;
    auto nextRand = [&rng]() { rng = rng * 6364136223846793005ull + 1442695040888963407ull; return (double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52); };

    // Fabrique un lot : x[B,T], cible one-hot [B*T, V]. Chaque séquence commence par
    // le TAG de sa langue (position 0), qui prédit le 1er caractère ; puis prédiction
    // caractère suivant. Langues réparties en round-robin sur le lot.
    auto makeBatch = [&](NkTensor& x, NkTensor& oneHot) {
        NkShape xs; xs.PushBack(B); xs.PushBack(T);
        x = NkTensor::Zeros(xs);
        oneHot = NkTensor::Zeros(NkShape{ B * T, (int64)V });
        float* xp = x.DataAs<float>(); float* op = oneHot.DataAs<float>();
        const int nL = (int)langData.size();
        for (int64 b = 0; b < B; ++b) {
            const int li = nL > 0 ? (int)(b % nL) : 0;
            const std::vector<float>& dd = langData[li];
            const int64 N = (int64)dd.size();
            if (N <= T) continue;
            const int64 off = (int64)(nextRand() * (double)(N - T));   // besoin de dd[off+T-1]
            xp[b*T + 0] = (float)(nByte + li);                         // tag de langue
            op[(b*T + 0) * V + (int)dd[off]] = 1.f;                    // le tag prédit le 1er car.
            for (int64 t = 1; t < T; ++t) {
                xp[b*T + t] = dd[off + t - 1];
                op[(b*T + t) * V + (int)dd[off + t]] = 1.f;
            }
        }
    };

    // Génération autoregressive (température). langIdx >= 0 => préfixe le tag de langue
    // pour piloter la langue générée. Les tokens-tag sont masqués à l'échantillonnage.
    auto generate = [&](const std::string& seed, int nToks, double temp, int langIdx) -> std::string {
        std::vector<int> ctx;
        if (langIdx >= 0 && langIdx < (int)langs.size()) ctx.push_back(nByte + langIdx);  // tag
        std::vector<int> seedIds; bpe.Encode(seed, seedIds);
        for (int id : seedIds) ctx.push_back(id);
        if (ctx.empty()) ctx.push_back(0);
        std::string out = seed;
        std::vector<float> logitBuf(V);
        for (int i = 0; i < nToks; ++i) {   // nToks = nombre de TOKENS BPE générés
            int64 len = (int64)ctx.size(); if (len > T) len = T;
            NkTensor tok = NkTensor::Zeros(NkShape{ (int64)1, len });
            float* tp = tok.DataAs<float>();
            for (int64 t = 0; t < len; ++t) tp[t] = (float)ctx[ctx.size() - len + t];
            NkVar logits = gpt.Forward(useGpu ? tok.ToGPU() : tok);   // [len, V]
            NkTensor lc = logits.Value().ToCPU().Contiguous();
            const float* lp = lc.DataAs<float>() + (len - 1) * V;     // dernière position
            // softmax(logits/temp) sur les OCTETS seulement (tags masqués).
            double mx = -1e30; for (int v = 0; v < nByte; ++v) if (lp[v] > mx) mx = lp[v];
            double sum = 0;
            for (int v = 0; v < V; ++v) {
                if (v >= nByte) { logitBuf[v] = 0.f; continue; }       // masque les tags
                double e = std::exp((lp[v] - mx) / temp); logitBuf[v] = (float)e; sum += e;
            }
            double r = nextRand() * sum, acc = 0; int next = 0;
            for (int v = 0; v < nByte; ++v) { acc += logitBuf[v]; if (acc >= r) { next = v; break; } }
            ctx.push_back(next);
            out += bpe.Decode(next);
        }
        return out;
    };

    const int GENLEN = (int)envI("NK_GPT_GENLEN", 400);

    // ---- Mode CHARGEMENT : on génère et on sort (aucun entraînement) ----
    if (envLoad) {
        printf("\n=== TEXTE GÉNÉRÉ (langue %s, amorce « %s », %d car.) ===\n",
               genLang >= 0 ? langs[genLang].c_str() : "auto", seed.c_str(), GENLEN);
        printf("%s\n", generate(seed, GENLEN, 0.8, genLang).c_str());
        printf("=========================================================\n");
        gpu.Shutdown();
        return 0;
    }

    // ---- Brique 10 : entraînement ----
    optim::NkAdam adam(params, 3e-4f, 0.9f, 0.999f, 1e-8f, /*weightDecay=AdamW*/ 0.01f);
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
            printf("    --- échantillons (pas %d) ---\n", s);
            if (langs.empty()) printf("    %s\n", generate(seed, 120, 0.8, -1).c_str());
            else for (int li = 0; li < (int)langs.size(); ++li)
                printf("    [%s] %s\n", langs[li].c_str(), generate(seed, 100, 0.8, li).c_str());
            printf("    ---------------------------\n");
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    printf("Entraînement terminé en %.1f s (%s).\n", std::chrono::duration<double>(t1 - t0).count(), useGpu ? "GPU-résident" : "CPU");

    // ---- Sauvegarde du modèle (si NK_GPT_SAVE) ----
    if (envSave) {
        GptMeta meta; meta.V = V; meta.d = (int32)d; meta.H = (int32)H; meta.L = (int32)L; meta.T = (int32)T; meta.langs = langs;
        for (auto& ab : bpe.merges) meta.merges.push_back(std::make_pair((int32)ab.first, (int32)ab.second));
        if (SaveCheckpoint(envSave, meta, params)) printf("Modèle sauvegardé : %s\n", envSave);
        else                                       printf("Échec de la sauvegarde : %s\n", envSave);
    }

    // ---- Génération finale (une par langue si multilingue) ----
    printf("\n=== TEXTE GÉNÉRÉ (amorce « %s », %d car., temp 0.8) ===\n", seed.c_str(), GENLEN);
    if (langs.size() <= 1) printf("%s\n", generate(seed, GENLEN, 0.8, langs.empty() ? -1 : 0).c_str());
    else for (int li = 0; li < (int)langs.size(); ++li)
        printf("[%s] %s\n\n", langs[li].c_str(), generate(seed, GENLEN, 0.8, li).c_str());
    printf("=========================================================\n");

    bool ok = ema < 3.0;   // la perte a nettement baissé depuis ~ln(V)
    printf("\n[%s] le GPT a appris (perte %.2f) et génère du texte %s.\n",
           ok ? " OK " : "FAIL", ema, useGpu ? "100%% sur GPU" : "sur CPU");
    gpu.Shutdown();
    return ok ? 0 : 1;
}
