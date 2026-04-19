// =============================================================================
// FICHIER  : Sandbox/System/NKFileSystem/src/main.cpp
// PROJET   : SandboxNKFileSystem
// OBJET    : Validation compile + execution du module NKFileSystem
// =============================================================================

#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"

#include <cstdio>

using namespace nkentseu;

int main() {
    // -- Build d'un repertoire de test local ---------------------------------
    NkPath root = NkPath("Build") / "SandboxNKFileSystem";

    if (!NkDirectory::CreateRecursive(root.CStr()) && !NkDirectory::Exists(root.CStr())) {
        std::printf("[SandboxNKFileSystem] echec CreateRecursive: %s\n", root.CStr());
        return 1;
    }

    // -- Ecriture / lecture d'un fichier -------------------------------------
    NkPath filePath = root / "smoke.txt";

    if (!NkFile::WriteAllText(filePath.CStr(), "nkfilesystem-ok")) {
        std::printf("[SandboxNKFileSystem] echec WriteAllText\n");
        return 2;
    }

    NkString readBack = NkFile::ReadAllText(filePath.CStr());
    if (readBack != "nkfilesystem-ok") {
        std::printf("[SandboxNKFileSystem] contenu inattendu: %s\n", readBack.CStr());
        return 3;
    }

    // -- Verification metadonnees --------------------------------------------
    NkFileAttributes attrs = NkFileSystem::GetFileAttributes(filePath.CStr());
    NkVector<NkDriveInfo> drives = NkFileSystem::GetDrives();

    std::printf("[SandboxNKFileSystem] file=%s size=%lld readonly=%d drives=%zu\n",
                filePath.CStr(),
                static_cast<long long>(NkFile::GetFileSize(filePath.CStr())),
                attrs.IsReadOnly ? 1 : 0,
                static_cast<size_t>(drives.Size()));

    // -- Cleanup --------------------------------------------------------------
    NkFile::Delete(filePath.CStr());
    NkDirectory::Delete(root.CStr(), true);

    return 0;
}
