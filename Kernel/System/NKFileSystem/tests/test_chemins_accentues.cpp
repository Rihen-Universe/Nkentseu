// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_chemins_accentues.cpp — NKFileSystem sur un chemin ACCENTUE (22/10).
//
// Le contrat du moteur : un chemin `const char*` est en UTF-8. Sous Windows, les API « A » (GetFileAttributesA,
// CreateDirectoryA, FindFirstFileA...) le lisaient dans la page de code locale : un dossier « Été 2026 » etait
// introuvable (mesure du 22/10 : « Ouvrir avec NKCode » sur un dossier accentue n'ouvrait rien). Elles passent par
// l'UTF-16 (NkFsLarge.h). Ce banc eprouve, sur un chemin a accents, espaces et ligature : la creation, l'existence,
// l'ecriture et la lecture, le listage (noms rendus en UTF-8 exact), les attributs et les dates, le chemin absolu,
// la surveillance (Windows), la suppression.
// Contre-epreuve : NK_FS_MUTATION=ansi (tout par la page de code locale, le comportement d'avant) -> ROUGE.
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKFileSystem/NkFileWatcher.h"

#if defined(_WIN32)
	#include <windows.h>
	#ifdef GetFileAttributes // la macro de windows.h masquerait NkFileSystem::GetFileAttributes
		#undef GetFileAttributes
	#endif
#endif

using namespace nkentseu;

namespace {
	/// Le dossier de travail : sous le dossier temporaire, un nom a accents, espaces et ligature.
	NkString NkTcaRacine() {
		return NkPath::GetTempDirectory().ToString() + "/nk_fs_accents";
	}
	NkString NkTcaDossier() {
		return NkTcaRacine() + "/\xC3\x89t\xC3\xA9 2026 \xC3\xA0 l'\xC5\x93il"; // « Été 2026 à l'œil »
	}
	bool NkTcaContient(const NkVector<NkString> &v, const NkString &nom) {
		for (usize i = 0; i < v.Size(); ++i)
			if (NkPath(v[i].CStr()).GetFileName() == nom)
				return true;
		return false;
	}

	class NkTcaTemoin : public NkFileWatcherCallback {
		public:
			int evenements = 0;
			bool vuLeNom = false;
			void OnFileChanged(const NkFileChangeEvent &e) override {
				++evenements;
				if (e.Path.Contains("nouveau \xC3\xA9.txt"))
					vuLeNom = true;
			}
	};
} // namespace

TEST_CASE(NKFileSystemAccents, CreerEcrireListerSupprimer) {
	const NkString dossier = NkTcaDossier();
	const NkString nomFichier("fichier \xC3\xA9t\xC3\xA9.txt"); // « fichier été.txt »
	const NkString fichier = dossier + "/" + nomFichier;
	NkDirectory::Delete(NkTcaRacine().CStr(), true);

	// creation et existence d'un dossier accentue
	ASSERT_TRUE(NkDirectory::CreateRecursive(dossier.CStr()));
	ASSERT_TRUE(NkDirectory::Exists(dossier.CStr()));

	// ecriture, existence et relecture d'un fichier accentue
	ASSERT_TRUE(NkFile::WriteAllText(fichier.CStr(), "contenu-\xC3\xA9"));
	ASSERT_TRUE(NkFile::Exists(fichier.CStr()));
	ASSERT_TRUE(NkFile::ReadAllText(fichier.CStr()) == NkString("contenu-\xC3\xA9"));

	// listage : le fichier et le dossier, leurs noms en UTF-8 EXACT
	ASSERT_TRUE(NkTcaContient(NkDirectory::GetFiles(dossier.CStr(), "*"), nomFichier));
	ASSERT_TRUE(NkTcaContient(NkDirectory::GetFiles(dossier.CStr(), "*.txt"), nomFichier));
	ASSERT_TRUE(NkTcaContient(NkDirectory::GetDirectories(NkTcaRacine().CStr(), "*"), NkPath(dossier.CStr()).GetFileName()));
	{
		const NkVector<NkDirectoryEntry> es = NkDirectory::GetEntries(dossier.CStr(), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
		bool vu = false;
		for (usize i = 0; i < es.Size(); ++i)
			if (es[i].Name == nomFichier && es[i].IsFile)
				vu = true;
		ASSERT_TRUE(vu);
	}

	// attributs et dates
	ASSERT_TRUE(NkFileSystem::GetLastWriteTime(fichier.CStr()) > 0);
	ASSERT_TRUE(NkFileSystem::SetReadOnly(fichier.CStr(), true));
	ASSERT_TRUE(NkFileSystem::GetFileAttributes(fichier.CStr()).IsReadOnly);
	ASSERT_TRUE(NkFileSystem::SetReadOnly(fichier.CStr(), false));
	ASSERT_TRUE(!NkFileSystem::GetFileAttributes(fichier.CStr()).IsReadOnly);

	// le chemin absolu garde ses accents
	ASSERT_TRUE(NkFileSystem::GetAbsolutePath(fichier.CStr()).Contains("\xC3\x89t\xC3\xA9 2026"));

	// suppression
	ASSERT_TRUE(NkFile::Delete(fichier.CStr()));
	ASSERT_TRUE(!NkFile::Exists(fichier.CStr()));
	ASSERT_TRUE(NkDirectory::Delete(dossier.CStr(), false));
	ASSERT_TRUE(!NkDirectory::Exists(dossier.CStr()));
	NkDirectory::Delete(NkTcaRacine().CStr(), true);
}

#if defined(_WIN32)
TEST_CASE(NKFileSystemAccents, SurveillerUnDossierAccentue) {
	const NkString dossier = NkTcaDossier();
	NkDirectory::Delete(NkTcaRacine().CStr(), true);
	ASSERT_TRUE(NkDirectory::CreateRecursive(dossier.CStr()));
	NkTcaTemoin temoin;
	NkFileWatcher veille(dossier.CStr(), &temoin, false);
	ASSERT_TRUE(veille.Start());
	::Sleep(200); // la veille s'installe
	ASSERT_TRUE(NkFile::WriteAllText((dossier + "/nouveau \xC3\xA9.txt").CStr(), "x"));
	for (int i = 0; i < 100 && !temoin.vuLeNom; ++i)
		::Sleep(20);
	veille.Stop();
	ASSERT_TRUE(temoin.vuLeNom);
	NkDirectory::Delete(NkTcaRacine().CStr(), true);
}
#endif
