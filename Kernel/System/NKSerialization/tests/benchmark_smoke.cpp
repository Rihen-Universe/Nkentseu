// =============================================================================
// tests/benchmark_smoke.cpp
// =============================================================================
// Benchmark minimal standalone (sans framework de test).
// =============================================================================
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/Binary/NkBinaryWriter.h"
#include "NKSerialization/Binary/NkBinaryReader.h"
#include "NKSerialization/Native/NkNativeFormat.h"
#include <cstdio>
#include <chrono>

using namespace nkentseu;
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

static void BenchJSON(int iters) {
    NkArchive archive;
    archive.SetString("name",  "Nkentseu");
    archive.SetUInt64("build", 20260413ull);
    archive.SetBool("debug",   true);
    archive.SetFloat64("scale",1.25);

    NkString payload;
    NkArchive decoded;
    size_t sink = 0;

    auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) {
        archive.SetInt64("iter", static_cast<int64>(i));
        NkJSONWriter::WriteArchive(archive, payload, false);
        sink += payload.Length();
        NkJSONReader::ReadArchive(payload.View(), decoded, nullptr);
        int64 v = 0;
        if (decoded.GetInt64("iter", v)) sink += static_cast<size_t>(v & 1);
    }
    auto t1 = Clock::now();
    double ms = Ms(t1 - t0).count();
    printf("[Bench] JSON encode+decode x%d: %.2f ms total  (%.4f ms/op)  sink=%zu\n",
           iters, ms, ms / iters, sink);
}

static void BenchBinary(int iters) {
    NkArchive archive;
    archive.SetInt64("a",  -42);
    archive.SetString("b", "hello binary world");

    NkVector<nk_uint8> bytes;
    NkArchive decoded;
    size_t sink = 0;

    auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) {
        archive.SetInt64("iter", static_cast<int64>(i));
        NkBinaryWriter::WriteArchive(archive, bytes);
        sink += bytes.Size();
        NkBinaryReader::ReadArchive(bytes.Data(), bytes.Size(), decoded, nullptr);
        int64 v = 0;
        if (decoded.GetInt64("iter", v)) sink += static_cast<size_t>(v & 1);
    }
    auto t1 = Clock::now();
    double ms = Ms(t1 - t0).count();
    printf("[Bench] Binary encode+decode x%d: %.2f ms total  (%.4f ms/op)  sink=%zu\n",
           iters, ms, ms / iters, sink);
}

static void BenchNative(int iters) {
    NkArchive archive;
    archive.SetString("label",  "native benchmark");
    archive.SetUInt64("id",     0xDEADBEEFull);
    archive.SetFloat64("ratio", 0.12345678901234);

    NkVector<nk_uint8> data;
    NkArchive decoded;
    NkString err;
    size_t sink = 0;

    auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) {
        archive.SetInt64("iter", static_cast<int64>(i));
        native::NkNativeWriter::WriteArchive(archive, data);
        sink += data.Size();
        native::NkNativeReader::ReadArchive(data.Data(), data.Size(), decoded, &err);
        int64 v = 0;
        if (decoded.GetInt64("iter", v)) sink += static_cast<size_t>(v & 1);
    }
    auto t1 = Clock::now();
    double ms = Ms(t1 - t0).count();
    printf("[Bench] NkNative encode+decode x%d: %.2f ms total  (%.4f ms/op)  sink=%zu\n",
           iters, ms, ms / iters, sink);
}

int main() {
    const int ITERS = 100000;
    printf("======================================================\n");
    printf("  NKSerialization Benchmark  (%d iterations)\n", ITERS);
    printf("======================================================\n");
    BenchJSON(ITERS);
    BenchBinary(ITERS);
    BenchNative(ITERS);
    printf("======================================================\n");
    return 0;
}
