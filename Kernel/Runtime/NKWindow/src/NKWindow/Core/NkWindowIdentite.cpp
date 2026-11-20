// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWindowIdentite.cpp — voir NkWindowIdentite.h pour le pourquoi.
// (19/11/2026, NKCode 1.7.0) Repris du commit c17d825d (branche famille/maj-capture-aumid), la partie identite
// seule, relue : sans la bibliotheque standard (copies et recherches a la main, env::GetEnvVar), et une
// contre-epreuve de banc (NK_NKWINDOW_MUTATION=identite-unique : l'identifiant unique d'avant, pour tous).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkEnv.h"
#include "NKWindow/Core/NkWindowIdentite.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
#define NK_IDENTITE_WIN32 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h> // SetCurrentProcessExplicitAppUserModelID
#elif defined(__APPLE__)
#include <stdlib.h> // getprogname
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h> // readlink
#endif

namespace nkentseu {

	namespace {
		/// L'identifiant declare par l'application (vide = calcul par defaut).
		char gDeclare[129] = {};
		bool gPose = false;

		bool NkEstSur(char c) noexcept {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
		}
		/// Copie bornee, toujours terminee (la place de strncpy).
		void NkCopier(char *dst, usize taille, const char *src) noexcept {
			usize i = 0;
			for (; src && src[i] && i + 1 < taille; ++i)
				dst[i] = src[i];
			if (taille > 0)
				dst[i] = 0;
		}
		bool NkEgal(const char *a, const char *b) noexcept {
			if (!a || !b)
				return false;
			while (*a && *a == *b) {
				++a;
				++b;
			}
			return *a == *b;
		}

#if defined(NK_IDENTITE_WIN32)
		NkString NkUtf8De(const wchar_t *w) {
			if (w == nullptr || w[0] == 0) {
				return NkString();
			}
			char tampon[1024] = {};
			const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, tampon, static_cast<int>(sizeof(tampon)), nullptr, nullptr);
			return n > 0 ? NkString(tampon) : NkString();
		}
#endif
	} // namespace

	void NkWindowDeclarerIdentite(const char *identifiant) noexcept {
		if (identifiant == nullptr) {
			gDeclare[0] = 0;
			return;
		}
		NkCopier(gDeclare, sizeof(gDeclare), identifiant);
	}

	NkString NkWindowNomExecutable() noexcept {
		char chemin[1024] = {};
#if defined(NK_IDENTITE_WIN32)
		wchar_t w[MAX_PATH * 2] = {};
		const DWORD n = GetModuleFileNameW(nullptr, w, static_cast<DWORD>(sizeof(w) / sizeof(w[0])));
		if (n == 0) {
			return NkString();
		}
		const NkString u = NkUtf8De(w);
		NkCopier(chemin, sizeof(chemin), u.CStr());
#elif defined(__APPLE__)
		NkCopier(chemin, sizeof(chemin), getprogname());
#elif defined(__linux__) || defined(__ANDROID__)
		const ssize_t n = readlink("/proc/self/exe", chemin, sizeof(chemin) - 1);
		if (n <= 0) {
			return NkString();
		}
		chemin[n] = 0;
#endif
		// Le dernier segment, sans extension.
		const char *debut = chemin;
		for (const char *p = chemin; *p; ++p) {
			if (*p == '/' || *p == '\\') {
				debut = p + 1;
			}
		}
		char nom[256] = {};
		NkCopier(nom, sizeof(nom), debut);
		char *point = nullptr;
		for (char *p = nom; *p; ++p)
			if (*p == '.')
				point = p;
		if (point != nullptr && point != nom) {
			*point = 0;
		}
		return NkString(nom);
	}

	NkString NkWindowSegmentIdentite(const char *nom) noexcept {
		NkString s;
		if (nom == nullptr) {
			return s;
		}
		int32 n = 0;
		for (const char *p = nom; *p && n < 64; ++p, ++n) {
			s += NkEstSur(*p) ? *p : '-';
		}
		return s;
	}

	NkString NkWindowIdentitePourExecutable(const char *nomExecutable) noexcept {
		const NkString seg = NkWindowSegmentIdentite(nomExecutable);
		NkString id("Rihen.Nkentseu.");
		id += seg.Empty() ? NkString("Application") : seg;
		return id;
	}

	NkString NkWindowIdentiteCalculee() noexcept {
		// 1. L'environnement (promesse de l'ancien code : elle reste tenue).
		const char *var = env::GetEnvVar("NkAppUserModelID");
		usize lg = 0;
		while (var != nullptr && var[lg] != 0 && lg < 200)
			++lg;
		if (var != nullptr && lg > 0 && lg < 128) {
			return NkString(var);
		}
		// Contre-epreuve de banc : l'identifiant unique d'avant le 02/10, pour TOUS les executables.
		if (NkEgal(env::GetEnvVar("NK_NKWINDOW_MUTATION"), "identite-unique")) {
			return NkString("Rihen.Nkentseu.Pong");
		}
		// 2. Ce que l'application a declare.
		if (gDeclare[0] != 0) {
			return NkString(gDeclare);
		}
		// 3. Le nom de l'executable : deux exe distincts, deux identites.
		const NkString exe = NkWindowNomExecutable();
		return NkWindowIdentitePourExecutable(exe.CStr());
	}

	bool NkWindowAppliquerIdentite() noexcept {
		if (gPose) {
			return true;
		}
		gPose = true;
#if defined(NK_IDENTITE_WIN32)
		const NkString id = NkWindowIdentiteCalculee();
		wchar_t w[256] = {};
		if (MultiByteToWideChar(CP_UTF8, 0, id.CStr(), -1, w, 256) <= 0) {
			return false;
		}
		return SUCCEEDED(SetCurrentProcessExplicitAppUserModelID(w));
#else
		return true;
#endif
	}

	NkString NkWindowIdentitePosee() noexcept {
#if defined(NK_IDENTITE_WIN32)
		PWSTR w = nullptr;
		if (FAILED(GetCurrentProcessExplicitAppUserModelID(&w)) || w == nullptr) {
			return NkString();
		}
		const NkString u = NkUtf8De(w);
		CoTaskMemFree(w);
		return u;
#else
		return NkString();
#endif
	}

} // namespace nkentseu
