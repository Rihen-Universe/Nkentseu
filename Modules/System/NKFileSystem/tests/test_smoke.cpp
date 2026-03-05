#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"

#include <cstdio>
#include <string>

using namespace nkentseu::entseu;

TEST_CASE(NKFileSystemSmoke, PathHelpers) {
    NkPath p("root");
    p.Append("folder").Append("file.txt");

    ASSERT_TRUE(p.HasExtension());
    ASSERT_TRUE(p.HasFileName());
    ASSERT_TRUE(std::string(p.GetExtension().CStr()) == ".txt");
}

TEST_CASE(NKFileSystemSmoke, FileAndDirectoryOps) {
    const std::string base = std::string("nk_fs_test_dir");
    const std::string filePath = base + "/sample.txt";

    NkDirectory::CreateRecursive(base.c_str());
    ASSERT_TRUE(NkDirectory::Exists(base.c_str()));

    ASSERT_TRUE(NkFile::WriteAllText(filePath.c_str(), "hello-fs"));
    ASSERT_TRUE(NkFile::Exists(filePath.c_str()));

    const auto text = NkFile::ReadAllText(filePath.c_str());
    ASSERT_TRUE(std::string(text.CStr()) == "hello-fs");

    ASSERT_TRUE(NkFile::Delete(filePath.c_str()));
    ASSERT_TRUE(NkDirectory::Delete(base.c_str(), false));
}
