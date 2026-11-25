# Les exemples de NKPlatform

Des programmes complets, compilés et exécutés à chaque construction de la documentation, avec ce qu'ils affichent.


Chaque exemple est un fichier de `Docs/exemples/Foundation/NKPlatform/`. Jenga le construit comme un programme console, puis `jenga docs exemples` le lance et compare sa sortie à ce qui est attendu. Un exemple qui ne compile plus, ou qui n'affiche plus la même chose, bloque la construction du site.

## Savoir pour quoi le programme a été compilé

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : les macros de détection, fixées À LA COMPILATION.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à detection.attendu.
// Il n'affiche que des faits vrais sur toutes les machines où la documentation se construit (Windows et Linux,
// 64 bits, C++20) : le nom exact de la plateforme change d'une machine à l'autre.

#include "NKPlatform/NKPlatform.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// Une macro de détection est DÉFINIE ou non : on la teste avec #if defined(...), jamais par sa valeur.
#if defined(NKENTSEU_ARCH_64BIT)
	const bool bits64 = true;
#else
	const bool bits64 = false;
#endif
	NkPrintln("Architecture 64 bits : {}", bits64 ? "oui" : "non");

#if defined(NKENTSEU_PLATFORM_DESKTOP)
	const bool bureau = true;
#else
	const bool bureau = false;
#endif
	NkPrintln("Système de bureau : {}", bureau ? "oui" : "non");

	// NKENTSEU_PLATFORM_NAME est une chaîne lisible ("Windows", "Linux"...), pour un journal ou un rapport.
	const char *nom = NKENTSEU_PLATFORM_NAME;
	NkPrintln("Nom de la plateforme connu : {}", (nom && nom[0] != 0) ? "oui" : "non");

	NkPrintln("C++20 ou plus récent : {}", NKENTSEU_CPP_VERSION >= 20 ? "oui" : "non");

	return (bits64 && bureau) ? 0 : 1;
}
```

Sortie :

```text
Architecture 64 bits : oui
Système de bureau : oui
Nom de la plateforme connu : oui
C++20 ou plus récent : oui
```


## Interroger le processeur à l'exécution

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : ce que le processeur sait faire, lu À L'EXÉCUTION.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à processeur.attendu.
// Il n'affiche que des faits vrais sur toutes les machines où la documentation se construit (x86-64 et ARM 64) :
// les valeurs exactes (nombre de cœurs, nom du processeur) changent d'une machine à l'autre.

#include "NKPlatform/NkCPUFeatures.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// NkGetCPUFeatures interroge le processeur une seule fois (au premier appel), puis garde le résultat.
	const platform::NkCPUFeatures &cpu = platform::NkGetCPUFeatures();

	const int32_t physiques = platform::NkGetPhysicalCoreCount();
	const int32_t logiques = platform::NkGetLogicalCoreCount();
	NkPrintln("Au moins un cœur physique : {}", physiques >= 1 ? "oui" : "non");
	NkPrintln("Autant ou plus de cœurs logiques que physiques : {}", logiques >= physiques ? "oui" : "non");

	// x86-64 garantit SSE2, ARM 64 bits garantit NEON : l'un des deux répond toujours oui sur nos cibles.
	const bool simd = platform::NkHasSSE2() || platform::NkHasNEON();
	NkPrintln("SSE2 ou NEON disponible : {}", simd ? "oui" : "non");

	// Une ligne de cache fait une puissance de deux octets (64 le plus souvent).
	const int32_t ligne = platform::NkGetCacheLineSize();
	const bool puissanceDeDeux = ligne > 0 && (ligne & (ligne - 1)) == 0;
	NkPrintln("Ligne de cache en puissance de deux : {}", puissanceDeDeux ? "oui" : "non");

	// Un deuxième appel rend le MÊME objet : la détection n'est pas refaite.
	const bool memeObjet = &cpu == &platform::NkGetCPUFeatures();
	NkPrintln("Le même résultat à chaque appel : {}", memeObjet ? "oui" : "non");

	return (physiques >= 1 && simd && puissanceDeDeux && memeObjet) ? 0 : 1;
}
```

Sortie :

```text
Au moins un cœur physique : oui
Autant ou plus de cœurs logiques que physiques : oui
SSE2 ou NEON disponible : oui
Ligne de cache en puissance de deux : oui
Le même résultat à chaque appel : oui
```


## Rediriger le journal de fondation

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : le journal de fondation, redirigé vers TON puits.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à journal_fondation.attendu.
// Par défaut, NK_FOUNDATION_LOG_* écrit sur la sortie d'erreur avec le fichier et la ligne ; un puits personnalisé
// reçoit chaque message et en fait ce qu'il veut (ici : une ligne sur la sortie standard, sans le chemin du fichier).

#include "NKPlatform/NkFoundationLog.h"
#include "NKContainers/String/NkPrint.h"

namespace {

	// Un puits reçoit le niveau, le fichier, la ligne et le message déjà formaté.
	void MonPuits(const char *niveau, const char * /*fichier*/, int /*ligne*/, const char *message) {
		nkentseu::NkPrintln("[{}] {}", niveau, message);
	}

} // namespace

int main() {
	using namespace nkentseu;

	platform::NkFoundationSetLogSink(&MonPuits);
	NK_FOUNDATION_LOG_WARN("le disque est presque plein");
	NK_FOUNDATION_LOG_ERROR("le fichier de configuration est illisible");

	// nullptr rend la sortie par défaut ; on vérifie que le puits a bien été retiré.
	platform::NkFoundationSetLogSink(nullptr);
	NkPrintln("Puits retiré : {}", platform::NkFoundationGetLogSink() == nullptr ? "oui" : "non");

	return 0;
}
```

Sortie :

```text
[WRN] le disque est presque plein
[ERR] le fichier de configuration est illisible
Puits retiré : oui
```


## Lire et convertir l'ordre des octets

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : l'ordre des octets d'un entier, et son passage à l'ordre du réseau.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à ordre_des_octets.attendu.

#include "NKPlatform/NkEndianness.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// Le sens de la machine est connu dès la compilation : aucun test n'est fait à l'exécution.
	const bool petitBoutiste = platform::NkIsLittleEndian();
	NkPrintln("Cette machine est {}.", petitBoutiste ? "petit-boutiste" : "gros-boutiste");

	// Inverser les octets d'un entier de 32 bits : 11 22 33 44 devient 44 33 22 11.
	const uint32_t valeur = 0x11223344u;
	const uint32_t inverse = platform::NkByteSwap32(valeur);
	NkPrintln("0x{:08X} inversé donne 0x{:08X}", valeur, inverse);

	// L'ordre du réseau est gros-boutiste. HostToNetwork n'inverse que si la machine est petit-boutiste,
	// et NetworkToHost défait exactement ce que HostToNetwork a fait.
	const uint32_t envoye = platform::NkHostToNetwork32(valeur);
	const uint32_t recu = platform::NkNetworkToHost32(envoye);
	NkPrintln("Envoyé sur le réseau : 0x{:08X}, relu : 0x{:08X}", envoye, recu);

	return recu == valeur ? 0 : 1;
}
```

Sortie :

```text
Cette machine est petit-boutiste.
0x11223344 inversé donne 0x44332211
Envoyé sur le réseau : 0x44332211, relu : 0x11223344
```


## Lire une variable d'environnement, reconnaître une variable absente

```cpp
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

	// GetEnvVar rend un pointeur vers la valeur, ou nullptr si la variable n'existe pas.
	// PATH existe partout où un programme se lance : on ne montre pas sa valeur, qui dépend de la machine.
	const char *chemin = env::GetEnvVar("PATH");
	NkPrintln("PATH est définie : {}", chemin ? "oui" : "non");

	// Teste TOUJOURS le pointeur avant de le lire : une variable absente ne rend pas une chaîne vide.
	const char *absente = env::GetEnvVar("NK_DOC_VARIABLE_QUI_N_EXISTE_PAS");
	NkPrintln("NK_DOC_VARIABLE_QUI_N_EXISTE_PAS : {}", absente ? absente : "(absente)");

	return (chemin && !absente) ? 0 : 1;
}
```

Sortie :

```text
PATH est définie : oui
NK_DOC_VARIABLE_QUI_N_EXISTE_PAS : (absente)
```


## Écrire, lire et retirer une variable (version de travail)

```cpp
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

	// Le constructeur de NkEnvString est explicite : on nomme la variable une fois.
	const env::NkEnvString nom("NK_DOC_EXEMPLE");
	// NkSet écrit pour CE processus (et ceux qu'il lancera), jamais pour la session de l'utilisateur.
	const bool ecrite = env::NkSet(nom, env::NkEnvString("bonjour"));
	NkPrintln("Écrite : {}", ecrite ? "oui" : "non");

	// NkGet remplit `valeur` ; une variable absente donne une chaîne vide. NkExists distingue « absente » de « vide ».
	env::NkEnvString valeur;
	env::NkGet(nom, valeur);
	NkPrintln("Lue : {}", valeur.CStr());

	env::NkUnset(nom);
	const bool encore = env::NkExists(nom);
	NkPrintln("Après retrait : {}", encore ? "encore là" : "(absente)");

	return (ecrite && !encore) ? 0 : 1;
}
```

Sortie :

```text
Écrite : oui
Lue : bonjour
Après retrait : (absente)
```

