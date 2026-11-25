// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : le résumé lisible du processeur, écrit par CPUFeatures::ToString.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à processeur_resume.attendu.
// Montré seulement dans la documentation de travail (dev) : en 0.1.0-beta.11, ToString était défini « inline » dans
// NkCPUFeatures.cpp, absent de NKPlatform.lib, et un programme hors du module ne se liait pas (corrigé le 25/11/2026).
// Le texte du résumé change d'une machine à l'autre : on ne vérifie que sa forme.

#include "NKPlatform/NkCPUFeatures.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;
	const platform::NkCPUFeatures &cpu = platform::NkGetCPUFeatures();

	// [extrait:tampon]
	// ToString écrit dans TON tampon et s'arrête à sa taille : rien n'est alloué.
	char resume[1024];
	cpu.ToString(resume, sizeof(resume));
	bool termine = false;
	for (size_t i = 0; i < sizeof(resume); ++i) {
		if (resume[i] == 0) {
			termine = true;
			break;
		}
	}
	NkPrintln("Résumé écrit : {}", resume[0] != 0 ? "oui" : "non");
	NkPrintln("Terminé dans le tampon : {}", termine ? "oui" : "non");
	// [/extrait]

	// [extrait:pratique]
	// Sans argument, ToString rend un tampon interne : pratique pour un journal, à recopier pour le garder.
	const char *texte = cpu.ToString();
	NkPrintln("Version sans tampon : {}", (texte && texte[0] != 0) ? "oui" : "non");
	// [/extrait]

	return (resume[0] != 0 && termine && texte && texte[0] != 0) ? 0 : 1;
}
