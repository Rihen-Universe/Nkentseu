// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkFsLarge.h — LES CHEMINS UTF-8 VERS LES API LARGES DE WINDOWS (22/10, interne a NKFileSystem).
//
// CONTRAT DU MOTEUR : un chemin `const char*` est en UTF-8. Les API « A » de Windows (GetFileAttributesA,
// CreateDirectoryA, FindFirstFileA, CreateFileA...) lisent leurs octets dans la PAGE DE CODE LOCALE : un dossier
// « Été 2026 » y est introuvable. Mesure du 22/10 (NKCode, « Ouvrir avec NKCode » sur « ...\Dossier à espaces ») :
// l'argument arrivait juste, NkDirectory::Exists repondait faux, rien ne s'ouvrait.
// On convertit donc en UTF-16 et on appelle la variante « W ». Validation STRICTE (MB_ERR_INVALID_CHARS) : des octets
// qui ne sont pas de l'UTF-8 valide (un appelant qui passerait encore la page de code locale) sont alors convertis
// par la PAGE DE CODE LOCALE (CP_ACP) -- exactement ce que l'API « A » faisait d'eux : rien de ce qui marchait ne
// cesse de marcher, sans recopier chaque appel en double.
// Contre-epreuve des bancs : la variable d'environnement NK_FS_MUTATION=ansi convertit TOUT par la page de code
// locale (le comportement d'avant : l'API « A » partout).
// =============================================================================
#if defined(_WIN32)
	#include <windows.h>

namespace nkentseu {

	/// La contre-epreuve (NK_FS_MUTATION=ansi), lue une fois.
	inline bool NkFsMutationAnsi() {
		static int etat = -1;
		if (etat < 0) {
			char v[16] = {0};
			const DWORD n = GetEnvironmentVariableA("NK_FS_MUTATION", v, static_cast<DWORD>(sizeof(v)));
			etat = (n > 0 && n < sizeof(v) && v[0] == 'a' && v[1] == 'n' && v[2] == 's' && v[3] == 'i' && v[4] == 0) ? 1 : 0;
		}
		return etat == 1;
	}

	/// Un chemin UTF-8 en UTF-16 (repli : la page de code locale, comme l'API « A »). `ok` faux seulement pour un
	/// chemin nul ou trop long : l'appelant rend alors l'echec qu'il aurait rendu.
	struct NkFsLarge {
			static const int kMax = 4096; ///< NTFS accepte bien plus que MAX_PATH avec les API larges
			wchar_t w[kMax];
			bool ok = false;
			explicit NkFsLarge(const char *chemin) noexcept {
				w[0] = 0;
				if (!chemin)
					return;
				ok = !NkFsMutationAnsi() && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, chemin, -1, w, kMax) > 0;
				if (!ok)
					ok = MultiByteToWideChar(CP_ACP, 0, chemin, -1, w, kMax) > 0;
			}
	};

	/// UTF-16 vers UTF-8 (un nom rendu par une API large). Faux si la place manque.
	inline bool NkFsVersUtf8(const wchar_t *w, char *out, int cap) noexcept {
		if (!w || !out || cap <= 1)
			return false;
		return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, nullptr, nullptr) > 0;
	}

	/// « . » ou « .. » (en UTF-16).
	inline bool NkFsPointOuPoints(const wchar_t *n) noexcept {
		return n[0] == L'.' && (n[1] == L'\0' || (n[1] == L'.' && n[2] == L'\0'));
	}

} // namespace nkentseu
#endif
