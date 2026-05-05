#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"

#include <cstdio>

using namespace nkentseu::entseu;
using nkentseu::NkString;

TEST_CASE(NKFileSystemSmoke, PathHelpers) {
    NkPath p("root");
    p.Append("folder").Append("file.txt");

    ASSERT_TRUE(p.HasExtension());
    ASSERT_TRUE(p.HasFileName());
    ASSERT_TRUE(p.GetExtension() == ".txt");
}

TEST_CASE(NKFileSystemSmoke, FileAndDirectoryOps) {
    const NkString base("nk_fs_test_dir");
    const NkString filePath = base + "/sample.txt";

    NkDirectory::CreateRecursive(base.CStr());
    ASSERT_TRUE(NkDirectory::Exists(base.CStr()));

    ASSERT_TRUE(NkFile::WriteAllText(filePath.CStr(), "hello-fs"));
    ASSERT_TRUE(NkFile::Exists(filePath.CStr()));

    const auto text = NkFile::ReadAllText(filePath.CStr());
    ASSERT_TRUE(text == "hello-fs");

    ASSERT_TRUE(NkFile::Delete(filePath.CStr()));
    ASSERT_TRUE(NkDirectory::Delete(base.CStr(), false));
}
