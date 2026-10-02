// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWindowIdentite.cpp — voir NkWindowIdentite.h pour le pourquoi.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkWindowIdentite.h"

#include <cstdlib>
#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
#define NK_IDENTITE_WIN32 1
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
		std::strncpy(gDeclare, identifiant, sizeof(gDeclare) - 1);
		gDeclare[sizeof(gDeclare) - 1] = 0;
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
		std::strncpy(chemin, u.CStr(), sizeof(chemin) - 1);
#elif defined(__APPLE__)
		const char *p = getprogname();
		if (p != nullptr) {
			std::strncpy(chemin, p, sizeof(chemin) - 1);
		}
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
		std::strncpy(nom, debut, sizeof(nom) - 1);
		char *point = std::strrchr(nom, '.');
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
		const char *env = std::getenv("NkAppUserModelID");
		if (env != nullptr && env[0] != 0 && std::strlen(env) < 128) {
			return NkString(env);
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
