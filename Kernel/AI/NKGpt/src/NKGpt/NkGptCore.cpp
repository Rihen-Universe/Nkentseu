// =============================================================================
// NKGpt/NkGptCore.cpp — implémentation des briques réutilisables (voir NkGptCore.h)
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKGpt/NkGptCore.h"
#include "NKFileSystem/NkDirectory.h"

#include <cstdio>

namespace nkentseu { namespace ai { namespace gpt {

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

NkString LoadCorpus(const char* path, nk_size maxChars) {
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

NkString LangOf(const NkString& path) {
    const char* p = path.Data(); int64 n = (int64)path.Size();
    int64 base = 0;
    for (int64 i = 0; i < n; ++i) if (p[i] == '/' || p[i] == '\\') base = i + 1;
    for (int64 i = base; i < n; ++i) {
        if (p[i] == '_') { int64 len = i - base; if (len >= 1 && len <= 4) return path.SubStr((NkString::SizeType)base, (NkString::SizeType)len); break; }
    }
    return NkString("??");
}

void LoadCorpusByLang(const NkString& dir, nk_size totalCap,
                      NkVector<NkString>& langs, NkVector<NkString>& texts) {
    NkVector<NkString> files = NkDirectory::GetFiles(dir.CStr(), "*.txt",
                                                     NkSearchOption::NK_TOP_DIRECTORY_ONLY);
    for (int64 i = 1; i < (int64)files.Size(); ++i)
        for (int64 j = i; j > 0; --j) {
            if (!(files[(nk_size)j] < files[(nk_size)(j - 1)])) break;
            NkString tmp = files[(nk_size)j]; files[(nk_size)j] = files[(nk_size)(j - 1)]; files[(nk_size)(j - 1)] = tmp;
        }
    if (files.Size() == 0) return;

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

// ================= I64Map =====================================================
static const int64 kEmpty = (int64)0x8000000000000000LL;

void I64Map::Init(int64 pow2) {
    keys.Clear(); vals.Clear();
    keys.Reserve((nk_size)pow2); vals.Reserve((nk_size)pow2);
    for (int64 i = 0; i < pow2; ++i) { keys.PushBack(kEmpty); vals.PushBack(0); }
    mask = pow2 - 1; bestKey = -1; bestVal = 0;
}
void I64Map::Reset() {
    for (int64 i = 0; i <= mask; ++i) { keys[(nk_size)i] = kEmpty; vals[(nk_size)i] = 0; }
    bestKey = -1; bestVal = 0;
}
uint64 I64Map::Hash(int64 k) { uint64 h = (uint64)k * 1099511628211ULL; h ^= h >> 29; h *= 1099511628211ULL; h ^= h >> 32; return h; }
void I64Map::Add(int64 k, int64 w) {
    int64 s = (int64)(Hash(k) & (uint64)mask);
    while (keys[(nk_size)s] != kEmpty && keys[(nk_size)s] != k) s = (s + 1) & mask;
    if (keys[(nk_size)s] == kEmpty) { keys[(nk_size)s] = k; vals[(nk_size)s] = w; }
    else vals[(nk_size)s] += w;
    int64 nv = vals[(nk_size)s];
    if (nv > bestVal) { bestVal = nv; bestKey = k; }
}
int64 I64Map::Get(int64 k, int64 def) const {
    int64 s = (int64)(Hash(k) & (uint64)mask);
    while (keys[(nk_size)s] != kEmpty) { if (keys[(nk_size)s] == k) return vals[(nk_size)s]; s = (s + 1) & mask; }
    return def;
}

static int64 PairKey(int32 a, int32 b) { return ((int64)a << 21) | (int64)b; }

// ================= BPE ========================================================
void Bpe::BuildVocabRank() {
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

void Bpe::PreTok(const NkString& text, NkVector<NkString>& words) {
    const char* p = text.Data(); int64 n = (int64)text.Size();
    NkString cur;
    for (int64 i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') { if (cur.Size() > 0) words.PushBack(cur); cur = NkString(); cur.Append((char)c); }
        else cur.Append((char)c);
    }
    if (cur.Size() > 0) words.PushBack(cur);
}

void Bpe::EncodeWord(const NkString& w, NkVector<int32>& out) const {
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
void Bpe::Encode(const NkString& text, NkVector<int32>& out) const {
    NkVector<NkString> words; PreTok(text, words);
    for (int64 i = 0; i < (int64)words.Size(); ++i) EncodeWord(words[(nk_size)i], out);
}

void TrainBpe(const NkVector<NkString>& texts, int nMerges, Bpe& bpe) {
    const int32 SEP = -1;
    const int64 CAP = 800000;
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

// ================= Checkpoint « NKGP » v3 =====================================
static int64 ShapeNumel(const NkShape& sh) { int64 n = 1; for (uint32 i = 0; i < sh.Size(); ++i) n *= sh[i]; return n; }

bool SaveCheckpoint(const char* path, const GptMeta& m, const NkVector<NkVar>& params) {
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

bool LoadCheckpointMeta(const char* path, GptMeta& m) {
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

bool LoadCheckpointWeights(const char* path, NkVector<NkVar>& params) {
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

} } } // namespace nkentseu::ai::gpt
