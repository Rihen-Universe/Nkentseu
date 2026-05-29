/**
 * @File   TestAudioFormats.cpp
 * @Brief  Test MP3 sur .mpga + OGG skeleton sur .ogg.
 * @Author TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */
#include "NKAudio/NKAudio.h"
#include "NKAudio/Codecs/MP3/NkMP3Codec.h"
#include "NKAudio/Codecs/OGG/NkOGGVorbisCodec.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NkAllocator.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::audio;

// Lit un fichier via NkFile + decode via decoder, sauve en WAV
static void TestFile(const char* path, const char* codec, const char* outWav) {
    std::printf("\n=== %s : %s ===\n", codec, path);

    NkFile f;
    if (!f.Open(path, NkFileMode::NK_READ_BINARY)) {
        std::printf("  FAIL : ouverture impossible\n");
        return;
    }
    nk_int64 size = f.GetSize();
    uint8* buf = (uint8*)memory::NkAlloc(usize(size), nullptr, sizeof(uint8));
    f.Read(buf, usize(size));
    f.Close();
    std::printf("  Lu : %lld octets\n", (long long)size);

    AudioSample s{};
    if (codec[0] == 'M') {
        s = NkMP3Codec::Decode(buf, usize(size), nullptr);
    } else {
        s = NkOGGVorbisCodec::Decode(buf, usize(size), nullptr);
    }
    memory::NkFree(buf, nullptr);

    if (!s.IsValid()) {
        std::printf("  Sample invalide (v0 OGG : attendu — decode complet en sessions futures)\n");
        return;
    }
    std::printf("  OK : %llu frames, %d ch, %d Hz, duree=%.2fs\n",
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
    std::printf("  Stats : min=%.4f max=%.4f avg=%.6f nonZero=%d/%llu\n",
                mn, mx, float(sum / double(n)), nNonZero, (unsigned long long)n);

    if (outWav && AudioLoader::SaveWAV(outWav, s)) {
        std::printf("  Sauve %s\n", outWav);
    }
    AudioLoader::Free(s);
}

int main() {
    // Test MP3 decoder sur .mpga (MPEG-1 Audio = MP3 avec autre extension)
    TestFile("Resources/Audio/Dark Heart.mpga",       "MP3", "Build/test_dark_heart.wav");
    TestFile("Resources/Audio/Face The Future.mpga",  "MP3", "Build/test_face_future.wav");
    TestFile("Resources/Audio/Innovating Care.mpga",  "MP3", "Build/test_innovating.wav");
    TestFile("Resources/Audio/Tranquility.mpga",      "MP3", "Build/test_tranquility.wav");

    // Test OGG skeleton sur .ogg
    TestFile("Resources/Audio/file_example_OOG_1MG.ogg", "OGG", nullptr);
    TestFile("Resources/Audio/file_example_OOG_2MG.ogg", "OGG", nullptr);
    TestFile("Resources/Audio/file_example_OOG_5MG.ogg", "OGG", nullptr);

    return 0;
}
