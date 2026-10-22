// =============================================================================
// NKCore/Text/NkSortie.h
// La sortie standard et la sortie d'erreur, sans libc : les équivalents de
// fputs, puts, fputc, fwrite, fflush, printf et fprintf.
//
// Rodolf (22/10/2026) : « tu penses que tu peux recoder std::fputs, std::puts,
// etc., pour les rendre aussi performants que l'original ? »
//
// D'où vient la vitesse de la libc, et ce que ce module en reprend :
//  1. UN TAMPON par flux : les octets s'accumulent en mémoire, et un seul appel
//     système (WriteFile sous Windows, write ailleurs) écrit tout le paquet.
//  2. LA POLITIQUE DE VIDAGE de la libc :
//       - vers une console : vidé après chaque appel qui écrit une fin de ligne ;
//       - vers un fichier ou un tube : vidé quand le tampon est plein ;
//       - la sortie d'erreur : jamais retenue (vidée à chaque appel) ;
//       - à la fin du programme : tout ce qui reste est écrit.
//  3. MOINS DE TRAVAIL que la libc : ni locale, ni conversion « mode texte »
//     (« \n » reste « \n », comme un flux binaire), un verrou rotatif minimal.
//     Vers la console Windows, l'UTF-8 passe en UTF-16 (WriteConsoleW) : les
//     accents s'affichent justes quelle que soit la page de codes.
//
// Mélanger ces fonctions et printf/fputs de la libc DANS UN MÊME PROGRAMME peut
// changer l'ordre des lignes quand la sortie va dans un fichier : chacun a son
// tampon. On convertit un programme entier, pas une ligne sur deux. NkFflush
// avant un appel à la libc remet l'ordre.
//
// Valeurs de retour : celles de la libc (fputs, puts : >= 0 si tout est écrit,
// -1 sinon ; fputc : le caractère, ou -1 ; fwrite : le nombre d'octets écrits ;
// printf : le nombre d'octets, ou -1).
//
// Le banc contre la libc : NKCore/bancs/banc_sortie_libc.cpp et banc_sortie.sh.
// Mesuré le 22/10/2026 (Windows, clang 18 de NKCode, un million de lignes) :
// plus rapide que la libc dans les 15 cas (fputs, puts, printf, fputc, fwrite ;
// fichier, NUL, tube), rapport de 0,09 à 0,85 ; octets identiques à la libc
// (77,9 Mo) ; 1 189 appels système au lieu d'un par ligne. Console Windows :
// accents relus justes (la libc affiche « ├ » pour « é »), y compris un « é » à
// cheval sur deux blocs ; la mutation du découpage est repérée.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-10-22
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once
#ifndef NK_CORE_TEXT_NKSORTIE_H_INCLUDED
#define NK_CORE_TEXT_NKSORTIE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkCoreApi.h"
#include "NKCore/Text/NkSnprintf.h" // NK_PRINTF_FORMAT
#include <cstdarg>					// va_list : fourni par le compilateur, pas par la libc

namespace nkentseu {

	/// Les deux flux : stdout et stderr.
	enum class NkFlux : uint32 {
		Sortie = 1,
		Erreur = 2
	};

	/// fputs : écrit le texte (sans fin de ligne).
	NKENTSEU_CORE_API int NkFputs(const char *texte, NkFlux flux) noexcept;

	/// puts : écrit le texte puis « \n » sur la sortie standard.
	NKENTSEU_CORE_API int NkPuts(const char *texte) noexcept;

	/// fputc / putchar : écrit un octet.
	NKENTSEU_CORE_API int NkFputc(int c, NkFlux flux) noexcept;

	/// fwrite : écrit `taille` octets. Rend le nombre d'octets écrits.
	NKENTSEU_CORE_API usize NkFwrite(const void *donnees, usize taille, NkFlux flux) noexcept;

	/// fflush : écrit ce que le tampon retient. 0, ou -1 en cas d'échec.
	NKENTSEU_CORE_API int NkFflush(NkFlux flux) noexcept;

	/// Vide les deux flux (appelé aussi à la fin du programme).
	NKENTSEU_CORE_API void NkFflushTout() noexcept;

	/// fprintf / vfprintf / printf : le format de NkSnprintf.
	NKENTSEU_CORE_API int NkVfprintf(NkFlux flux, const char *format, va_list args) noexcept NK_PRINTF_FORMAT(2, 0);
	NKENTSEU_CORE_API int NkFprintf(NkFlux flux, const char *format, ...) noexcept NK_PRINTF_FORMAT(2, 3);
	NKENTSEU_CORE_API int NkPrintfSortie(const char *format, ...) noexcept NK_PRINTF_FORMAT(1, 2);

	/// Pour les bancs : la taille du tampon d'un flux (0 = sans tampon), et le
	/// nombre d'appels système faits depuis le début.
	NKENTSEU_CORE_API void NkSortieFixerTampon(NkFlux flux, usize taille) noexcept;
	NKENTSEU_CORE_API uint64 NkSortieAppelsSysteme(NkFlux flux) noexcept;

} // namespace nkentseu

#endif // NK_CORE_TEXT_NKSORTIE_H_INCLUDED
