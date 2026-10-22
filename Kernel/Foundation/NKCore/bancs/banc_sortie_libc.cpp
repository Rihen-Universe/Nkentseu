// =============================================================================
// NKCore/bancs/banc_sortie_libc.cpp
// Le banc de NkSortie CONTRE la libc : même travail, même destination, on
// compare le temps et les octets écrits.
//
//   banc_sortie_libc <mode> <operation> <n> <fichier-de-mesure>
//     mode      : nk | libc | nk0 (NkSortie SANS tampon : la contre-épreuve)
//     operation : fputs | puts | printf | fputc | fwrite
//     n         : nombre de lignes
//   La sortie standard est redirigée par banc_sortie.sh (fichier, NUL, tube).
//   La mesure (durée en microsecondes, appels système) va dans le fichier de
//   mesure, écrit par l'API Windows : elle ne passe ni par NkSortie ni par la
//   libc mesurées.
//
// Ce banc emploie la bibliothèque standard, et c'est voulu : elle est l'objet
// de la comparaison (comme NKContainers/tests/test_format_libc.cpp).
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-10-22
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "NKCore/Text/NkSortie.h"
#include "NKCore/Text/NkSnprintf.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>

using namespace nkentseu;

namespace {

	const char *kTexte = "le texte de la sortie standard, avec des accents : \xC3\xA9 \xC3\xA8 \xC3\xA0 \xC3\xA7";

	long long Maintenant() {
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return t.QuadPart;
	}

	void EcrireMesure(const char *chemin, const char *texte) {
		HANDLE h = CreateFileA(chemin, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return;
		DWORD ecrit = 0;
		WriteFile(h, texte, (DWORD)std::strlen(texte), &ecrit, nullptr);
		CloseHandle(h);
	}

} // namespace

int main(int argc, char **argv) {
	if (argc < 5)
		return 2;
	const char *mode = argv[1];
	const char *op = argv[2];
	const long n = std::atol(argv[3]);
	const bool nk = std::strcmp(mode, "libc") != 0;
	if (std::strcmp(mode, "nk0") == 0)
		NkSortieFixerTampon(NkFlux::Sortie, 0);
	else if (std::strncmp(mode, "nk", 2) == 0 && mode[2] >= '1' && mode[2] <= '9')
		NkSortieFixerTampon(NkFlux::Sortie, (usize)std::atol(mode + 2) * 1024u); // « nk16 » : 16 Kio
	if (!nk)
		_setmode(_fileno(stdout), _O_BINARY); // « \n » reste « \n », comme NkSortie

	// ── L'épreuve de la console : écrire des accents, puis les RELIRE dans la
	// mémoire de la console. À lancer dans une console neuve (cachée) :
	//   banc_sortie_libc nk|libc console 0 <fichier-de-mesure>
	// Ligne 1 : « accents : é è à ç ». Ligne 2 : 2047 « a » puis « é » : le « é »
	// tombe à cheval sur la limite des blocs de 2048 octets de NkSortie.
	if (std::strcmp(op, "console") == 0) {
		static char long_[2052];
		for (int i = 0; i < 2047; ++i)
			long_[i] = 'a';
		long_[2047] = '\xC3';
		long_[2048] = '\xA9';
		long_[2049] = '\n';
		if (nk) {
			NkFputs("accents : \xC3\xA9 \xC3\xA8 \xC3\xA0 \xC3\xA7\n", NkFlux::Sortie);
			NkFwrite(long_, 2050, NkFlux::Sortie);
			NkFflush(NkFlux::Sortie);
		} else {
			std::fputs("accents : \xC3\xA9 \xC3\xA8 \xC3\xA0 \xC3\xA7\n", stdout);
			std::fwrite(long_, 1, 2050, stdout);
			std::fflush(stdout);
		}
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO info = {};
		GetConsoleScreenBufferInfo(h, &info);
		wchar_t lu1[32] = {};
		DWORD nlu = 0;
		COORD c = {0, 0};
		ReadConsoleOutputCharacterW(h, lu1, 17, c, &nlu);
		const wchar_t attendu1[] = L"accents : é è à ç";
		bool juste1 = nlu == 17;
		for (int i = 0; i < 17 && juste1; ++i)
			juste1 = lu1[i] == attendu1[i];
		static wchar_t lu2[2100];
		c.Y = 1;
		ReadConsoleOutputCharacterW(h, lu2, 2048, c, &nlu);
		bool juste2 = nlu == 2048 && lu2[2047] == L'é';
		for (int i = 0; i < 2047 && juste2; ++i)
			juste2 = lu2[i] == L'a';
		char mesure[200];
		NkSnprintf(mesure, sizeof(mesure), "console %s : accents %s, limite de bloc %s (largeur %d, lu U+%04X)\n", mode,
				   juste1 ? "JUSTES" : "FAUX", juste2 ? "JUSTE" : "FAUX", (int)info.dwSize.X, (unsigned)lu1[10]);
		EcrireMesure(argv[4], mesure);
		return (juste1 && juste2) ? 0 : 3;
	}

	char ligne[160];
	const long long t0 = Maintenant();
	for (long i = 0; i < n; ++i) {
		if (std::strcmp(op, "fputs") == 0) {
			NkSnprintf(ligne, sizeof(ligne), "ligne %ld : %s\n", i, kTexte);
			if (nk)
				NkFputs(ligne, NkFlux::Sortie);
			else
				std::fputs(ligne, stdout);
		} else if (std::strcmp(op, "puts") == 0) {
			NkSnprintf(ligne, sizeof(ligne), "ligne %ld : %s", i, kTexte);
			if (nk)
				NkPuts(ligne);
			else
				std::puts(ligne);
		} else if (std::strcmp(op, "printf") == 0) {
			if (nk)
				NkPrintfSortie("ligne %ld : %s\n", i, kTexte);
			else
				std::printf("ligne %ld : %s\n", i, kTexte);
		} else if (std::strcmp(op, "fputc") == 0) {
			const int c = (i % 64 == 63) ? '\n' : ('a' + (int)(i % 26));
			if (nk)
				NkFputc(c, NkFlux::Sortie);
			else
				std::fputc(c, stdout);
		} else if (std::strcmp(op, "fwrite") == 0) {
			const int l = NkSnprintf(ligne, sizeof(ligne), "ligne %ld : %s\n", i, kTexte);
			if (nk)
				NkFwrite(ligne, (usize)l, NkFlux::Sortie);
			else
				std::fwrite(ligne, 1, (size_t)l, stdout);
		}
	}
	if (nk)
		NkFflush(NkFlux::Sortie);
	else
		std::fflush(stdout);
	const long long t1 = Maintenant();

	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	char mesure[160];
	NkSnprintf(mesure, sizeof(mesure), "%lld %llu\n", (t1 - t0) * 1000000LL / f.QuadPart,
			   (unsigned long long)NkSortieAppelsSysteme(NkFlux::Sortie));
	EcrireMesure(argv[4], mesure);
	return 0;
}
