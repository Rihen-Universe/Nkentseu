// =============================================================================
// NKCore/Text/NkSnprintf.h
// Facades NkSnprintf / NkVsnprintf : la semantique EXACTE de snprintf, sans libc.
//
//  - Retour : la longueur qui AURAIT ete ecrite (hors '\0'), meme tronquee.
//  - Troncature : au plus cap-1 caracteres, toujours termines par '\0'.
//  - buf == nullptr ou cap == 0 : rien n'est ecrit, la longueur est calculee
//    (le motif « mesurer puis allouer » de NkString::VFormat).
//
// Conversions : d i u x X o (b B en extension) c s p f F e E g G a A n %%.
// Longueurs : hh h l ll j z t L. Largeur et precision par '*'.
// Nomme, pas fait : positions « %1$d », %ls / %lc (caracteres larges),
// long double a 80 bits (converti en double), drapeau « ' » (ignore, comme la
// CRT de reference).
//
// Le coeur nombre -> texte est NkNumberToText.h ; le banc differentiel contre la
// libc est NKContainers/tests/test_format_libc.cpp.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-09-04
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NK_CORE_TEXT_NKSNPRINTF_H_INCLUDED
#define NK_CORE_TEXT_NKSNPRINTF_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkCoreApi.h"

#include <cstdarg> // va_list : fourni par le compilateur, pas par la libc

// Le compilateur verifiait les formats de snprintf (-Wformat) ; une fonction
// maison perd ce controle sauf a le declarer. GCC/clang : attribut format.
#if defined(__GNUC__) || defined(__clang__)
#define NK_PRINTF_FORMAT(fmtIndex, firstArg) __attribute__((format(printf, fmtIndex, firstArg)))
#else
#define NK_PRINTF_FORMAT(fmtIndex, firstArg)
#endif

namespace nkentseu {

	/// Equivalent de vsnprintf(buf, cap, fmt, ap), sans libc.
	NKENTSEU_CORE_API int NkVsnprintf(char *buf, usize cap, const char *fmt, va_list ap) NK_PRINTF_FORMAT(3, 0);

	/// Equivalent de snprintf(buf, cap, fmt, ...), sans libc.
	NKENTSEU_CORE_API int NkSnprintf(char *buf, usize cap, const char *fmt, ...) NK_PRINTF_FORMAT(3, 4);

} // namespace nkentseu

#endif // NK_CORE_TEXT_NKSNPRINTF_H_INCLUDED
