/**
 * @File   TestMP3.cpp
 * @Brief  Test MP3 decoder reel : sauve le WAV decode pour validation auditive.
 *         Utilise NkFile (NKFileSystem) -> heritage fallback AAssetManager Android.
 * @Author TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */
#include "NKAudio/NKAudio.h"
#include "NKAudio/Codecs/MP3/NkMP3Codec.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NkAllocator.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::audio;

static void TestOne(const char* path, const char* outWav) {
    std::printf("\n=== Test : %s ===\n", path);

    // Lecture via NkFile (gere Android AAssetManager automatiquement)
    NkFile f;
    if (!f.Open(path, NkFileMode::NK_READ_BINARY)) {
        std::printf("  FAIL : ouverture impossible\n");
        return;
    }
    nk_int64 size = f.GetSize();
    uint8* buf = (uint8*)memory::NkAlloc(usize(size), nullptr, sizeof(uint8));
    f.Read(buf, usize(size));
    f.Close();
    std::printf("  Fichier : %lld octets\n", (long long)size);

    AudioSample s = NkMP3Codec::Decode(buf, usize(size), nullptr);
    memory::NkFree(buf, nullptr);

    if (!s.IsValid()) {
        std::printf("  FAIL : decode returne sample invalide\n");
        return;
    }
    std::printf("  OK : %llu frames, %d canaux, %d Hz, duree=%.2fs\n",
                (unsigned long long)s.frameCount, s.channels, s.sampleRate,
                double(s.frameCount) / double(s.sampleRate));

    // Stats sur les 5000 premiers samples
    usize n = (s.frameCount > 5000 ? 5000 : s.frameCount) * usize(s.channels);
    float mn = 1e30f, mx = -1e30f;
    double sum = 0.0;
    int nNonZero = 0;
    for (usize i = 0; i < n; ++i) {
        float v = s.data[i];
        if (v != 0.0f) ++nNonZero;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += double(v);
    }
    std::printf("  Stats 5000 samples : min=%.4f max=%.4f avg=%.6f nonZero=%d/%llu\n",
                mn, mx, float(sum / double(n)), nNonZero, (unsigned long long)n);

    if (AudioLoader::SaveWAV(outWav, s)) {
        std::printf("  Sauve %s\n", outWav);
    } else {
        std::printf("  Echec save WAV\n");
    }
    AudioLoader::Free(s);
}

int main() {
    TestOne("Resources/Audio/bleep.mp3",    "Build/test_mp3_bleep.wav");
    TestOne("Resources/Audio/breakout.mp3", "Build/test_mp3_breakout.wav");
    return 0;
}
