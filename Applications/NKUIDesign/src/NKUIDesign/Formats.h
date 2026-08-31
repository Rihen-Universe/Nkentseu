#pragma once
// -----------------------------------------------------------------------------
// @File    Formats.h
// @Brief   LE CATALOGUE DES FORMATS DE PAGE (demande de Rodolf, 31/08 : « HD,
//          Full HD, tous les types d'écran PC, mobile et web possibles, du
//          custom, et même A4, A3... »). C'est de la DONNEE, pas du code : une
//          table declaree UNE fois, extensible — le jour ou l'utilisateur
//          ajoute les siens (la philosophie des composants), c'est ICI qu'ils
//          entrent, et l'interface boucle dessus sans changer.
//
//          ⚠️ LE PAPIER A SA CONVENTION, ECRITE ET AFFICHEE : les formats A*
//          sont en millimetres dans la vraie vie (A4 = 210 × 297 mm) — ils
//          entrent au catalogue en PIXELS A 96 DPI, le standard ecran
//          (210 mm = 8,268 po × 96 = 794 px). La note « @ 96 dpi » fait partie
//          du NOM DE CIBLE ecrit dans le document : un format papier qui ne
//          dit pas son dpi est un chiffre sans provenance. L'impression a
//          300 dpi sera une variante du catalogue, pas une reinterpretation
//          silencieuse de ces nombres.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include <NKGui/NKGui.h>

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;

	/// Un format de page du catalogue. `nom` est ce que la cible du document
	/// portera (suivi de « L x H ») ; `note` s'affiche ET s'ecrit quand elle
	/// existe (la provenance dpi du papier).
	struct NkFormatPage {
			const char *categorie; ///< « Mobile », « Tablette », « Bureau », « Web », « Papier »
			const char *nom;	   ///< « Full HD », « A4 », « iPhone 15 »...
			float32 w, h;		   ///< pixels logiques de la cible
			const char *note;	   ///< "" ou « @ 96 dpi » (papier)
	};

	/// LA table — un seul endroit. L'ordre des categories est l'ordre
	/// d'affichage ; dans une categorie, du plus courant au plus rare.
	inline const NkFormatPage *NkFormatCatalogue(uint32 &count) {
		static const NkFormatPage k[] = {
			// ── Mobile (portrait, les tailles courantes 2026) ──
			{"Mobile", "iPhone 15", 390.f, 844.f, ""},
			{"Mobile", "iPhone 15 Pro Max", 430.f, 932.f, ""},
			{"Mobile", "iPhone SE", 375.f, 667.f, ""},
			{"Mobile", "Android", 360.f, 800.f, ""},
			{"Mobile", "Android grand", 412.f, 915.f, ""},
			// ── Tablette ──
			{"Tablette", "iPad", 768.f, 1024.f, ""},
			{"Tablette", "iPad Pro 11", 834.f, 1194.f, ""},
			{"Tablette", "Android 10\"", 800.f, 1280.f, ""},
			// ── Bureau (les definitions d'ecran PC) ──
			{"Bureau", "HD", 1280.f, 720.f, ""},
			{"Bureau", "WXGA", 1366.f, 768.f, ""},
			{"Bureau", "Full HD", 1920.f, 1080.f, ""},
			{"Bureau", "QHD", 2560.f, 1440.f, ""},
			{"Bureau", "4K UHD", 3840.f, 2160.f, ""},
			// ── Web (les points de rupture usuels) ──
			{"Web", "Desktop 1440", 1440.f, 900.f, ""},
			{"Web", "Laptop 1280", 1280.f, 800.f, ""},
			{"Web", "Tablette 1024", 1024.f, 768.f, ""},
			{"Web", "Mobile 768", 768.f, 1024.f, ""},
			// ── Papier (mm -> px @ 96 dpi : 210x297 -> 794x1123, etc.) ──
			{"Papier", "A4 portrait", 794.f, 1123.f, "@ 96 dpi"},
			{"Papier", "A4 paysage", 1123.f, 794.f, "@ 96 dpi"},
			{"Papier", "A3 portrait", 1123.f, 1587.f, "@ 96 dpi"},
			{"Papier", "A3 paysage", 1587.f, 1123.f, "@ 96 dpi"},
			{"Papier", "A5 portrait", 559.f, 794.f, "@ 96 dpi"},
			{"Papier", "A5 paysage", 794.f, 559.f, "@ 96 dpi"},
		};
		count = (uint32)(sizeof(k) / sizeof(k[0]));
		return k;
	}

	/// Petite egalite locale (pas d'include de NkComponentDecl ici — le
	/// catalogue reste une table nue).
	inline bool NkFormatStrEq(const char *a, const char *b) {
		if (!a || !b)
			return a == b;
		while (*a && *a == *b) {
			++a;
			++b;
		}
		return *a == *b;
	}

	/// Les categories, dans l'ordre d'affichage, SANS doublon — derivees de la
	/// table (une categorie ajoutee a la table apparait toute seule).
	/// « Personnalisé » est ajoutee en queue par l'interface : elle n'est pas
	/// un preset, c'est le couple L×H libre.
	inline uint32 NkFormatCategories(const char **out, uint32 cap) {
		uint32 n = 0, total = 0;
		const NkFormatPage *k = NkFormatCatalogue(total);
		for (uint32 i = 0; i < total && n < cap; ++i) {
			bool vu = false;
			for (uint32 j = 0; j < n; ++j)
				if (NkFormatStrEq(out[j], k[i].categorie))
					vu = true;
			if (!vu)
				out[n++] = k[i].categorie;
		}
		return n;
	}

} // namespace nkuidesign
