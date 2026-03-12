#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/NkSerializer.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/Binary/NkBinaryReader.h"
#include "NKSerialization/Binary/NkBinaryWriter.h"

using namespace nkentseu;

TEST_CASE(NKSerializationSmoke, JsonRoundTripFlatArchive) {
    NkArchive source;
    ASSERT_TRUE(source.SetString("name", "Nkentseu"));
    ASSERT_TRUE(source.SetUInt64("build", 20260311ull));
    ASSERT_TRUE(source.SetBool("debug", true));
    ASSERT_TRUE(source.SetFloat64("scale", 1.25));

    NkString json;
    ASSERT_TRUE(NkJSONWriter::WriteArchive(source, json, true));
    ASSERT_TRUE(!json.Empty());

    NkArchive decoded;
    NkString err;
    ASSERT_TRUE(NkJSONReader::ReadArchive(json.View(), decoded, &err));
    ASSERT_TRUE(err.Empty());

    NkString name;
    uint64 build = 0;
    bool debug = false;
    double scale = 0.0;
    ASSERT_TRUE(decoded.GetString("name", name));
    ASSERT_TRUE(decoded.GetUInt64("build", build));
    ASSERT_TRUE(decoded.GetBool("debug", debug));
    ASSERT_TRUE(decoded.GetFloat64("scale", scale));

    ASSERT_EQUAL(name, NkString("Nkentseu"));
    ASSERT_EQUAL(build, static_cast<uint64>(20260311ull));
    ASSERT_TRUE(debug);
    ASSERT_TRUE(scale > 1.2 && scale < 1.3);
}

TEST_CASE(NKSerializationSmoke, BinaryRoundTripFlatArchive) {
    NkArchive source;
    ASSERT_TRUE(source.SetInt64("a", -42));
    ASSERT_TRUE(source.SetString("b", "hello"));

    NkVector<nk_uint8> bytes;
    ASSERT_TRUE(NkBinaryWriter::WriteArchive(source, bytes));
    ASSERT_TRUE(bytes.Size() > 0);

    NkArchive decoded;
    NkString err;
    ASSERT_TRUE(NkBinaryReader::ReadArchive(bytes.Data(), bytes.Size(), decoded, &err));
    ASSERT_TRUE(err.Empty());

    int64 a = 0;
    NkString b;
    ASSERT_TRUE(decoded.GetInt64("a", a));
    ASSERT_TRUE(decoded.GetString("b", b));
    ASSERT_EQUAL(a, static_cast<int64>(-42));
    ASSERT_EQUAL(b, NkString("hello"));
}
