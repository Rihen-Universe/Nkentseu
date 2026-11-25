// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : lire une variable d'environnement, et reconnaître une variable absente.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à
// variables_environnement.attendu. N'utilise que l'API présente dans la version publiée 0.1.0-beta.11 et
// utilisable hors du module : GetEnvVar (NkSet, NkGet, NkExists et NkUnset y sont déclarées « inline » sans
// définition dans l'en-tête : un programme qui les appelle ne se lie pas ; voir la page du manuel).

#include "NKPlatform/NkEnv.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// [extrait:lire]
	// GetEnvVar rend un pointeur vers la valeur, ou nullptr si la variable n'existe pas.
	// PATH existe partout où un programme se lance : on ne montre pas sa valeur, qui dépend de la machine.
	const char *chemin = env::GetEnvVar("PATH");
	NkPrintln("PATH est définie : {}", chemin ? "oui" : "non");
	// [/extrait]

	// [extrait:absente]
	// Teste TOUJOURS le pointeur avant de le lire : une variable absente ne rend pas une chaîne vide.
	const char *absente = env::GetEnvVar("NK_DOC_VARIABLE_QUI_N_EXISTE_PAS");
	NkPrintln("NK_DOC_VARIABLE_QUI_N_EXISTE_PAS : {}", absente ? absente : "(absente)");
	// [/extrait]

	return (chemin && !absente) ? 0 : 1;
}
