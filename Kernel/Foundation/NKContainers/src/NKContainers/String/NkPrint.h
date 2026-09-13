// -----------------------------------------------------------------------------
// FICHIER: Containers/NKString/NkPrint.h
// DESCRIPTION: Sorties console pratiques (NkPrint, NkPrintln, NkEPrint,
//              NkEPrintln) construites sur NkFormat.
//              Deplacees hors de NkFormat.h le 2026-09-04 : c'est de la SORTIE
//              (fputs/puts), pas du formatage en tampon. NkFormat.h n'inclut
//              plus <cstdio> ; ce fichier est le seul de la famille a le faire.
//              Aucun consommateur mesure a cette date (grep sur Kernel, Engine,
//              Applications) : API conservee a l'identique.
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_FORMAT_NKPRINT_H_INCLUDED
#define NK_FORMAT_NKPRINT_H_INCLUDED

#include "NkFormat.h"

#include <cstdio>

namespace nkentseu {

	template <typename... Args> void NkPrint(NkStringView fmt, const Args &...args) {
		std::fputs(NkFormat(fmt, args...).Data(), stdout);
	}

	template <typename... Args> void NkPrintln(NkStringView fmt, const Args &...args) {
		std::puts(NkFormat(fmt, args...).Data());
	}

	template <typename... Args> void NkEPrint(NkStringView fmt, const Args &...args) {
		std::fputs(NkFormat(fmt, args...).Data(), stderr);
	}

	template <typename... Args> void NkEPrintln(NkStringView fmt, const Args &...args) {
		std::fputs((NkFormat(fmt, args...) + "\n").Data(), stderr);
	}

} // namespace nkentseu

#endif // NK_FORMAT_NKPRINT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
