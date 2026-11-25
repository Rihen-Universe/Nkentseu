// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : les macros de détection, fixées À LA COMPILATION.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à detection.attendu.
// Il n'affiche que des faits vrais sur toutes les machines où la documentation se construit (Windows et Linux,
// 64 bits, C++20) : le nom exact de la plateforme change d'une machine à l'autre.

#include "NKPlatform/NKPlatform.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// [extrait:architecture]
	// Une macro de détection est DÉFINIE ou non : on la teste avec #if defined(...), jamais par sa valeur.
#if defined(NKENTSEU_ARCH_64BIT)
	const bool bits64 = true;
#else
	const bool bits64 = false;
#endif
	NkPrintln("Architecture 64 bits : {}", bits64 ? "oui" : "non");
	// [/extrait]

	// [extrait:bureau]
#if defined(NKENTSEU_PLATFORM_DESKTOP)
	const bool bureau = true;
#else
	const bool bureau = false;
#endif
	NkPrintln("Système de bureau : {}", bureau ? "oui" : "non");
	// [/extrait]

	// [extrait:nom]
	// NKENTSEU_PLATFORM_NAME est une chaîne lisible ("Windows", "Linux"...), pour un journal ou un rapport.
	const char *nom = NKENTSEU_PLATFORM_NAME;
	NkPrintln("Nom de la plateforme connu : {}", (nom && nom[0] != 0) ? "oui" : "non");
	// [/extrait]

	// [extrait:standard]
	NkPrintln("C++20 ou plus récent : {}", NKENTSEU_CPP_VERSION >= 20 ? "oui" : "non");
	// [/extrait]

	return (bits64 && bureau) ? 0 : 1;
}
