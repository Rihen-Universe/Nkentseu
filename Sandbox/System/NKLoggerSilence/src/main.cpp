// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Sandbox/System/NKLoggerSilence/src/main.cpp
//
// BANC : « logger.Debug() n'affiche rien, et meme std::cout n'affiche rien ».
//
// Il ne corrige rien : il MESURE, et il ecrit ce qu'il mesure sur stdout, sur
// stderr, et dans un fichier temoin. Chaque ligne porte un marqueur que le
// script de lecture retrouve. Ce qui n'apparait PAS est aussi une mesure : le
// script compte les marqueurs attendus et nomme ceux qui manquent.
//
// Trois questions, trois sections :
//   (A) y a-t-il une console, et stdout est-il tamponne ?
//   (B) que fait std::cout / printf sans vidage, dans une boucle qui ne
//       rend jamais la main ?
//   (C) que fait NKLogger sans configuration : combien de puits, quel
//       niveau, quel message sort ou ne sort pas ?
// =============================================================================

// ORDRE DES INCLUSIONS, ET IL COMPTE : NkLog.h definit la MACRO `logger`.
// NkRegistry.h, lui, nomme `logger` un PARAMETRE (`Register(... logger)`).
// Inclus apres NkLog.h, il ne compile pas -- la macro devient
// `::nkentseu::NkLog::Instance().Source(...)` au milieu d une declaration.
// Le registre passe donc EN PREMIER. (Le sandbox NKLogger existant fait
// deja cet ordre, sans dire pourquoi.)
#include "NKLogger/NkRegistry.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#endif

using namespace nkentseu;

// ---------------------------------------------------------------------------
// Ecrit une ligne DIRECTEMENT au descripteur 1, sans passer par la libc.
// C'est le seul canal qui ne peut pas etre tamponne : il sert de temoin
// « le processus a bien tourne » meme quand tout le reste est retenu.
// ---------------------------------------------------------------------------
static void EcrireSansTampon(const char *texte) {
#if defined(_WIN32)
	HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
	if (h != INVALID_HANDLE_VALUE && h != nullptr) {
		DWORD ecrits = 0;
		::WriteFile(h, texte, (DWORD)::strlen(texte), &ecrits, nullptr);
	}
#else
	(void)::write(1, texte, ::strlen(texte));
#endif
}

// ---------------------------------------------------------------------------
// LIT CE QUI EST REELLEMENT AFFICHE dans la console de CE processus, et le
// depose dans un fichier. C'est la seule facon honnete de repondre a « est-ce
// que l'utilisateur VOIT la ligne ? » sans photographier un ecran : on relit
// le tampon d'ecran de la console, caractere par caractere.
// ATTENTION : appele AVANT la sortie du programme : a ce moment la libc n'a
// encore rien vide de son propre chef, donc l'ecran montre exactement ce
// qu'une boucle de jeu qui ne finit jamais aurait montre.
// ---------------------------------------------------------------------------
#if defined(_WIN32)
static void RelireEcranConsole(const char *chemin) {
	HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == nullptr || hOut == INVALID_HANDLE_VALUE) {
		return;
	}
	CONSOLE_SCREEN_BUFFER_INFO info{};
	if (!::GetConsoleScreenBufferInfo(hOut, &info)) {
		return; // pas une console : rien a relire, et c'est deja une mesure
	}
	FILE *f = ::fopen(chemin, "wb");
	if (f == nullptr) {
		return;
	}
	const SHORT largeur = info.dwSize.X;
	const SHORT lignes = (SHORT)(info.dwCursorPosition.Y + 1);
	char *tampon = (char *)::malloc((size_t)largeur + 2);
	if (tampon == nullptr) {
		::fclose(f);
		return;
	}
	for (SHORT y = 0; y < lignes; ++y) {
		COORD depart{0, y};
		DWORD lus = 0;
		if (!::ReadConsoleOutputCharacterA(hOut, tampon, (DWORD)largeur, depart, &lus)) {
			break;
		}
		int fin = (int)lus;
		while (fin > 0 && (tampon[fin - 1] == ' ' || tampon[fin - 1] == 0)) {
			--fin;
		}
		tampon[fin] = 0;
		::fputs(tampon, f);
		::fputc(10, f);
	}
	::free(tampon);
	::fclose(f);
}
#endif

int main(int argc, char **argv) {
	bool avecBoucle = false;
	const char *cheminEcran = nullptr;
	for (int i = 1; i < argc; ++i) {
		if (::strcmp(argv[i], "--boucle") == 0) {
			avecBoucle = true;
		} else if (::strcmp(argv[i], "--ecran") == 0 && i + 1 < argc) {
			cheminEcran = argv[++i];
		}
	}

#if defined(_WIN32)
	// Fenetre de sonde : titree et inerte, conformement aux regles du depot.
	::SetConsoleTitleA("*** SONDE DE MESURE *** NKLoggerSilence");
#endif

	// -----------------------------------------------------------------------
	// (A) LA CONSOLE EXISTE-T-ELLE, ET DE QUEL TYPE EST stdout ?
	// -----------------------------------------------------------------------
	EcrireSansTampon("[BRUT] A00 ce processus ecrit au descripteur 1 sans libc\n");

#if defined(_WIN32)
	{
		char tampon[256];
		HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
		const DWORD typeOut = (hOut && hOut != INVALID_HANDLE_VALUE) ? ::GetFileType(hOut) : 0;
		// FILE_TYPE_CHAR=2 (console), FILE_TYPE_DISK=1 (fichier), FILE_TYPE_PIPE=3 (tube)
		const char *nomType = (typeOut == 2) ? "CHAR(console)"
							: (typeOut == 1) ? "DISK(fichier)"
							: (typeOut == 3) ? "PIPE(tube)"
											 : "INCONNU";
		::snprintf(tampon, sizeof(tampon), "[BRUT] A01 GetStdHandle=%p GetFileType=%lu %s\n",
				   (void *)hOut, (unsigned long)typeOut, nomType);
		EcrireSansTampon(tampon);

		const HWND fenetreConsole = ::GetConsoleWindow();
		::snprintf(tampon, sizeof(tampon), "[BRUT] A02 GetConsoleWindow=%p (nul = aucune console attachee)\n",
				   (void *)fenetreConsole);
		EcrireSansTampon(tampon);

		::snprintf(tampon, sizeof(tampon), "[BRUT] A03 _isatty(1)=%d\n", ::_isatty(1));
		EcrireSansTampon(tampon);
	}
#endif

	// A10 : la configuration de COMPILATION, telle que le preprocesseur la voit.
	//       C'est elle qui decide si NkLog branche un puits console (NkLog.cpp
	//       teste `!defined(NDEBUG)`), et il ne faut pas la deviner.
	EcrireSansTampon(
#if defined(NDEBUG)
		"[BRUT] A10 NDEBUG=defini"
#else
		"[BRUT] A10 NDEBUG=absent"
#endif
#if defined(_DEBUG)
		" _DEBUG=defini"
#else
		" _DEBUG=absent"
#endif
#if defined(NKENTSEU_DEBUG_CONSOLE)
		" NKENTSEU_DEBUG_CONSOLE=defini\n"
#else
		" NKENTSEU_DEBUG_CONSOLE=absent\n"
#endif
	);

	// -----------------------------------------------------------------------
	// (B) std::cout ET printf, SANS VIDAGE
	// -----------------------------------------------------------------------
	// B10 : ce que l'utilisateur a ecrit, mot pour mot — une chaine VIDE.
	std::cout << "";
	// B11 : une vraie chaine, SANS saut de ligne et SANS flush.
	std::cout << "[COUT] B11 cout sans saut de ligne et sans flush";
	// B12 : printf sans saut de ligne.
	::printf("[PRINTF] B12 printf sans saut de ligne");
	// B13 : cout avec saut de ligne, toujours sans flush explicite.
	std::cout << "\n[COUT] B13 cout avec saut de ligne, sans flush\n";
	// B14 : stderr, qui n'est pas tamponne.
	::fprintf(stderr, "[ERR] B14 fprintf vers stderr\n");

	// -----------------------------------------------------------------------
	// (C) NKLOGGER SANS AUCUNE CONFIGURATION
	// -----------------------------------------------------------------------
	{
		char tampon[256];
		NkLog &journal = NkLog::Instance();
		::snprintf(tampon, sizeof(tampon), "[BRUT] C00 GetSinkCount=%llu GetLevel=%s\n",
				   (unsigned long long)journal.GetSinkCount(),
				   NkLogLevelToString(journal.GetLevel()));
		EcrireSansTampon(tampon);
	}

	logger.Trace("[NKLOG] C10 trace");
	logger.Debug("[NKLOG] C11 debug");
	logger.Info("[NKLOG] C12 info");
	logger.Warn("[NKLOG] C13 warn");
	logger.Error("[NKLOG] C14 error");

	// -----------------------------------------------------------------------
	// (E) UN LOGGER NOMME, OBTENU PAR CreateLogger()
	// -----------------------------------------------------------------------
	// C'est la SECONDE porte du module, et elle ne donne pas sur la meme piece :
	// `NkRegistry::GetOrCreate` ne branche AUCUN puits. Un logger obtenu ainsi
	// accepte tous les appels et n'ecrit nulle part. On le mesure plutot que de
	// le deduire du code.
	{
		memory::NkSharedPtr<NkLogger> nomme = CreateLogger("banc-silence");
		char tampon[256];
		if (nomme) {
			::snprintf(tampon, sizeof(tampon), "[BRUT] E00 CreateLogger: GetSinkCount=%llu GetLevel=%s\n",
					   (unsigned long long)nomme->GetSinkCount(), NkLogLevelToString(nomme->GetLevel()));
			EcrireSansTampon(tampon);
			nomme->Info("[NKLOG] E10 info depuis un logger nomme");
			nomme->Error("[NKLOG] E11 error depuis un logger nomme");
		} else {
			EcrireSansTampon("[BRUT] E00 CreateLogger a rendu un pointeur nul\n");
		}
	}

	// -----------------------------------------------------------------------
	// (D) LA BOUCLE DE JEU : on n'atteint jamais la fin du programme
	// -----------------------------------------------------------------------
	// Sans `--boucle`, le programme se termine et la libc vide ses tampons a la
	// sortie : c'est le cas FACILE. Avec `--boucle`, il tourne un moment sans
	// rien vider, comme une boucle de jeu, puis s'arrete TOUT SEUL (jamais de
	// processus a tuer).
	if (avecBoucle) {
		EcrireSansTampon("[BRUT] D00 entree dans la boucle, 3 secondes\n");
		const unsigned long debut =
#if defined(_WIN32)
			::GetTickCount();
#else
			0;
#endif
		unsigned long tours = 0;
		for (;;) {
			std::cout << "[COUT] D10 dans la boucle, tour " << tours << "\n";
			logger.Debug("[NKLOG] D11 debug dans la boucle, tour {0}", (int)tours);
			logger.Info("[NKLOG] D12 info dans la boucle, tour {0}", (int)tours);
			++tours;
#if defined(_WIN32)
			::Sleep(200);
			if (::GetTickCount() - debut > 3000) {
				break;
			}
#else
			if (tours > 15) break;
#endif
		}
		char tampon[128];
		::snprintf(tampon, sizeof(tampon), "[BRUT] D20 sortie de boucle apres %lu tours\n", tours);
		EcrireSansTampon(tampon);
	}

	// On NE vide PAS explicitement : la sortie normale du programme le fera.
	// C'est exactement la difference avec une boucle de jeu qui ne finit pas.
	EcrireSansTampon("[BRUT] Z99 fin de main\n");
	
#if defined(_WIN32)
	// Relecture de l'ecran AVANT la sortie du programme : la libc n'a encore
	// rien vide, donc ce fichier contient EXACTEMENT ce que l'utilisateur voit.
	if (cheminEcran != nullptr) {
		RelireEcranConsole(cheminEcran);
	}
#endif
	return 0;
}
