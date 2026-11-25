// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : écrire, lire, tester puis retirer une variable d'environnement.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à
// environnement_ecrire.attendu. Montré seulement dans la documentation de travail (dev) : en 0.1.0-beta.11,
// NkSet, NkGet, NkExists et NkUnset étaient déclarées « inline » sans corps dans l'en-tête, et un programme hors du
// module ne se liait pas (corrigé le 25/11/2026).

#include "NKPlatform/NkEnv.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// [extrait:ecrire]
	// Le constructeur de NkEnvString est explicite : on nomme la variable une fois.
	const env::NkEnvString nom("NK_DOC_EXEMPLE");
	// NkSet écrit pour CE processus (et ceux qu'il lancera), jamais pour la session de l'utilisateur.
	const bool ecrite = env::NkSet(nom, env::NkEnvString("bonjour"));
	NkPrintln("Écrite : {}", ecrite ? "oui" : "non");
	// [/extrait]

	// [extrait:lire]
	// NkGet remplit `valeur` ; une variable absente donne une chaîne vide. NkExists distingue « absente » de « vide ».
	env::NkEnvString valeur;
	env::NkGet(nom, valeur);
	NkPrintln("Lue : {}", valeur.CStr());
	// [/extrait]

	// [extrait:retirer]
	env::NkUnset(nom);
	const bool encore = env::NkExists(nom);
	NkPrintln("Après retrait : {}", encore ? "encore là" : "(absente)");
	// [/extrait]

	return (ecrite && !encore) ? 0 : 1;
}
