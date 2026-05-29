/**
 * @File   TestFLAC.cpp
 * @Brief  Smoke test : charge un .flac reel via NkFile (Android-friendly) et stats.
 * @Author TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */
#include "NKAudio/NKAudio.h"
#include "NKAudio/Codecs/FLAC/NkFLACCodec.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NkAllocator.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::audio;

int main(int argc, char** argv) {
    const char* path = (argc > 1)
        ? argv[1]
        : "Resources/Audio/Fleetwood Mac - The Chain (2001 Remaster).flac";

    std::printf("[TestFLAC] Chargement %s ...\n", path);

    NkFile f;
    if (!f.Open(path, NkFileMode::NK_READ_BINARY)) {
        std::printf("[TestFLAC] FAIL : impossible d'ouvrir le fichier\n");
        return 1;
    }
    nk_int64 size = f.GetSize();
    uint8* buf = (uint8*)memory::NkAlloc(usize(size), nullptr, sizeof(uint8));
    f.Read(buf, usize(size));
    f.Close();

    std::printf("[TestFLAC] Fichier lu : %lld octets\n", (long long)size);

    AudioSample s = NkFLACCodec::Decode(buf, usize(size), nullptr);
    memory::NkFree(buf, nullptr);

    if (!s.IsValid()) {
        std::printf("[TestFLAC] FAIL : NkFLACCodec::Decode a retourne un AudioSample vide\n");
        return 2;
    }

    std::printf("[TestFLAC] OK : %llu frames, %d canaux, %d Hz\n",
                (unsigned long long)s.frameCount, s.channels, s.sampleRate);
    std::printf("[TestFLAC] Duree : %.2f secondes\n",
                double(s.frameCount) / double(s.sampleRate));

    // Stats min/max/avg sur les 1000 premiers samples
    usize n = (s.frameCount > 1000 ? 1000 : s.frameCount) * usize(s.channels);
    float mn = 1e30f, mx = -1e30f;
    double sum = 0.0;
    for (usize i = 0; i < n; ++i) {
        float v = s.data[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += double(v);
    }
    std::printf("[TestFLAC] Stats 1000 premiers samples : min=%.4f max=%.4f avg=%.4f\n",
                mn, mx, float(sum / double(n)));

    const char* outPath = "Build/test_flac_output.wav";
    if (AudioLoader::SaveWAV(outPath, s)) {
        std::printf("[TestFLAC] Sauve WAV : %s\n", outPath);
    } else {
        std::printf("[TestFLAC] Echec save WAV\n");
    }

    AudioLoader::Free(s);
    return 0;
}
