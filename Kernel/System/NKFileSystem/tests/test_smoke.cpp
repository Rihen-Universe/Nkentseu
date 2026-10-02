#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"

#include <cstdio>

using namespace nkentseu;
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

// LocateResource : le dossier courant d'abord (chemin ABSOLU rendu), puis en
// remontant ; une ressource absente rend une chaine vide.
TEST_CASE(NKFileSystemSmoke, LocateResource) {
	const char *kMarque = "nk_fs_locate_marque.txt";
	const NkString racine = NkPath::GetCurrentDirectory().ToString();
	ASSERT_TRUE(NkFile::WriteAllText(kMarque, "ici"));

	// 1. Dans le dossier courant : trouvee, et en chemin absolu.
	const NkString ici = NkPath::LocateResource(kMarque);
	ASSERT_FALSE(ici.Empty());
	ASSERT_TRUE(NkPath(ici).IsAbsolute());
	ASSERT_TRUE(NkFile::ReadAllText(ici.CStr()) == "ici");

	// 2. Deux dossiers plus bas : trouvee en REMONTANT, au meme endroit.
	ASSERT_TRUE(NkDirectory::CreateRecursive("nk_fs_locate/a/b"));
	ASSERT_TRUE(NkDirectory::SetCurrentDirectory("nk_fs_locate/a/b"));
	const NkString enHaut = NkPath::LocateResource(kMarque);
	// 3. Les variantes : la premiere absente, la seconde trouvee.
	const NkString variante = NkPath::LocateResource({"nk_fs_locate_absente.txt", kMarque});
	// 4. Absente partout : vide (sans avertissement au journal ici).
	const NkString absente = NkPath::LocateResource("nk_fs_locate_absente_9f3.txt", false);
	ASSERT_TRUE(NkDirectory::SetCurrentDirectory(racine.CStr()));

	// (Comparer le CONTENU, pas la chaine : la casse de « C: » peut differer
	// entre le dossier courant et celui de l'executable.)
	ASSERT_TRUE(NkPath(enHaut).IsAbsolute());
	ASSERT_TRUE(NkFile::ReadAllText(enHaut.CStr()) == "ici");
	ASSERT_TRUE(NkFile::ReadAllText(variante.CStr()) == "ici");
	ASSERT_TRUE(absente.Empty());

	ASSERT_TRUE(NkFile::Delete(kMarque));
	ASSERT_TRUE(NkDirectory::Delete("nk_fs_locate", true));
}
