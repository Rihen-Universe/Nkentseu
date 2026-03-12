#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkFoundationLog.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu;

static double NkToNs(const timespec& ts) {
    return (static_cast<double>(ts.tv_sec) * 1000000000.0) + static_cast<double>(ts.tv_nsec);
}

TEST_CASE(NKSerializationBenchmark, JsonEncodeDecodeFlatArchive) {
    constexpr int kIters = 300000;

    NkArchive archive;
    archive.SetString("name", "Nkentseu");
    archive.SetUInt64("build", 20260311ull);
    archive.SetBool("debug", true);
    archive.SetFloat64("scale", 1.25);

    nk_size sink = 0;
    NkString payload;

    timespec t0 {};
    timespec t1 {};
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < kIters; ++i) {
        archive.SetInt64("iter", static_cast<int64>(i));
        NkJSONWriter::WriteArchive(archive, payload, false);
        sink += payload.Length();

        NkArchive decoded;
        NkJSONReader::ReadArchive(payload.View(), decoded, nullptr);
        int64 iter = 0;
        if (decoded.GetInt64("iter", iter)) {
            sink += static_cast<nk_size>(iter & 0x1);
        }
    }
    timespec_get(&t1, TIME_UTC);

    const double ns = NkToNs(t1) - NkToNs(t0);
    ASSERT_TRUE(ns > 0.0);
    ASSERT_TRUE(sink > 0);
    NK_FOUNDATION_LOG_INFO("[NKSerialization Benchmark] json encode/decode loop: %.2f ns total (sink=%zu)",
                           ns, static_cast<size_t>(sink));
}
