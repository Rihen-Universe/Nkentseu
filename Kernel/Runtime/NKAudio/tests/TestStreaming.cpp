/**
 * @File   TestStreaming.cpp
 * @Brief  Test streaming : ouvre un fichier FLAC, joue 3 secondes via le player.
 */
#include "NKAudio/NKAudio.h"
#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>

using namespace nkentseu;
using namespace nkentseu::audio;

int main() {
    const char* path = "Resources/Audio/Fleetwood Mac - The Chain (2001 Remaster).flac";
    std::printf("[TestStreaming] Ouverture %s ...\n", path);

    IAudioStream* stream = OpenAudioStream(path);
    if (!stream) {
        std::printf("[TestStreaming] FAIL : OpenAudioStream\n");
        return 1;
    }
    std::printf("[TestStreaming] Stream ouvert : %lld frames, %d Hz, %d ch\n",
                (long long)stream->GetFrameCount(),
                stream->GetSampleRate(), stream->GetChannels());

    AudioStreamPlayer player;
    if (!player.Init(stream->GetSampleRate(), stream->GetChannels(), 88200)) {
        std::printf("[TestStreaming] FAIL : player.Init\n");
        delete stream;
        return 2;
    }

    if (!player.Play(stream, /*loop=*/false)) {
        std::printf("[TestStreaming] FAIL : player.Play\n");
        return 3;
    }

    // Pull 3 secondes audio par chunks de 1024 frames
    const int32 sampleRate = 44100;
    const int32 channels = 2;
    const int32 totalFrames = sampleRate * 3;  // 3 sec
    const int32 chunkFrames = 1024;
    float32* outBuf = (float32*)std::malloc(usize(totalFrames) * usize(channels) * sizeof(float32));
    if (!outBuf) {
        player.Shutdown();
        return 4;
    }

    int32 readSoFar = 0;
    int32 nZeroFrames = 0;
    while (readSoFar < totalFrames) {
        int32 want = totalFrames - readSoFar;
        if (want > chunkFrames) want = chunkFrames;
        int32 got = player.ReadFrames(outBuf + usize(readSoFar) * usize(channels), want);
        if (got == 0) {
            // Buffer vide : laisser le thread worker rattraper
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ++nZeroFrames;
            if (nZeroFrames > 1000) {
                std::printf("[TestStreaming] Timeout : trop d'attente buffer\n");
                break;
            }
        } else {
            readSoFar += got;
            // Simule le rythme du backend audio (10ms par chunk de 1024 frames @ 44.1kHz)
            std::this_thread::sleep_for(std::chrono::microseconds(int(got * 1000000LL / sampleRate)));
        }
    }
    std::printf("[TestStreaming] Lu %d / %d frames\n", readSoFar, totalFrames);

    // Stats sur ce qui a ete lu
    float mn = 1e30f, mx = -1e30f;
    double sum = 0.0;
    int nNonZero = 0;
    for (int i = 0; i < readSoFar * channels; ++i) {
        float v = outBuf[i];
        if (v != 0.0f) ++nNonZero;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += double(v);
    }
    std::printf("[TestStreaming] Stats : min=%.4f max=%.4f avg=%.6f nonZero=%d/%d\n",
                mn, mx, float(sum / double(readSoFar * channels)),
                nNonZero, readSoFar * channels);

    // Sauve les 3 secondes en WAV pour verification
    AudioSample s;
    s.data        = outBuf;
    s.frameCount  = usize(readSoFar);
    s.sampleRate  = sampleRate;
    s.channels    = channels;
    s.format      = AudioFormat::WAV;
    s.mAllocator  = nullptr;
    AudioLoader::SaveWAV("Build/test_streaming_output.wav", s);
    std::printf("[TestStreaming] Sauve Build/test_streaming_output.wav\n");

    std::free(outBuf);
    player.Shutdown();
    return 0;
}
