// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : ce que le processeur sait faire, lu À L'EXÉCUTION.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à processeur.attendu.
// Il n'affiche que des faits vrais sur toutes les machines où la documentation se construit (x86-64 et ARM 64) :
// les valeurs exactes (nombre de cœurs, nom du processeur) changent d'une machine à l'autre.

#include "NKPlatform/NkCPUFeatures.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// [extrait:lire]
	// NkGetCPUFeatures interroge le processeur une seule fois (au premier appel), puis garde le résultat.
	const platform::NkCPUFeatures &cpu = platform::NkGetCPUFeatures();
	// [/extrait]

	// [extrait:coeurs]
	const int32_t physiques = platform::NkGetPhysicalCoreCount();
	const int32_t logiques = platform::NkGetLogicalCoreCount();
	NkPrintln("Au moins un cœur physique : {}", physiques >= 1 ? "oui" : "non");
	NkPrintln("Autant ou plus de cœurs logiques que physiques : {}", logiques >= physiques ? "oui" : "non");
	// [/extrait]

	// [extrait:simd]
	// x86-64 garantit SSE2, ARM 64 bits garantit NEON : l'un des deux répond toujours oui sur nos cibles.
	const bool simd = platform::NkHasSSE2() || platform::NkHasNEON();
	NkPrintln("SSE2 ou NEON disponible : {}", simd ? "oui" : "non");
	// [/extrait]

	// [extrait:cache]
	// Une ligne de cache fait une puissance de deux octets (64 le plus souvent).
	const int32_t ligne = platform::NkGetCacheLineSize();
	const bool puissanceDeDeux = ligne > 0 && (ligne & (ligne - 1)) == 0;
	NkPrintln("Ligne de cache en puissance de deux : {}", puissanceDeDeux ? "oui" : "non");
	// [/extrait]

	// [extrait:unique]
	// Un deuxième appel rend le MÊME objet : la détection n'est pas refaite.
	const bool memeObjet = &cpu == &platform::NkGetCPUFeatures();
	NkPrintln("Le même résultat à chaque appel : {}", memeObjet ? "oui" : "non");
	// [/extrait]

	return (physiques >= 1 && simd && puissanceDeDeux && memeObjet) ? 0 : 1;
}
