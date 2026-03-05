#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKStream/NkBinaryStream.h"
#include "NKStream/NkFileStream.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;

TEST_CASE(NKStreamSmoke, BinaryStreamReadWrite) {
    NkBinaryStream stream(64);

    const int values[3] = {10, 20, 30};
    ASSERT_EQUAL(3, static_cast<int>(stream.Write(values, 3)));
    ASSERT_TRUE(stream.Seek(0));

    int out[3] = {0, 0, 0};
    ASSERT_EQUAL(3, static_cast<int>(stream.Read(out, 3)));
    ASSERT_EQUAL(20, out[1]);
}

TEST_CASE(NKStreamSmoke, FileStreamReadWrite) {
    const char* path = "nk_stream_test.tmp";

    NkFileStream w;
    ASSERT_TRUE(w.Open(path, NkStream::WriteMode | NkStream::BinaryMode));
    const char payload[] = "hello-stream";
    ASSERT_EQUAL(static_cast<int>(sizeof(payload) - 1), static_cast<int>(w.WriteRaw(payload, sizeof(payload) - 1)));
    w.Close();

    NkFileStream r;
    ASSERT_TRUE(r.Open(path, NkStream::ReadMode | NkStream::BinaryMode));
    char out[32] = {};
    ASSERT_EQUAL(static_cast<int>(sizeof(payload) - 1), static_cast<int>(r.ReadRaw(out, sizeof(payload) - 1)));
    ASSERT_EQUAL(0, std::strcmp(payload, out));
    r.Close();

    std::remove(path);
}
