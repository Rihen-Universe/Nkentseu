# Ton premier programme avec NKPlatform

En cinq étapes, tu déclares un programme console dans Jenga, tu lis l'ordre des octets de ta machine, tu passes un entier à l'ordre du réseau, puis tu lis une variable d'environnement.


Tout le code de ce tutoriel est réel : ce sont deux programmes de `Docs/exemples/`, compilés et exécutés à chaque construction de cette documentation. Ce que tu lis sous « Sortie » est ce qu'ils ont affiché.

## Étape 1 : déclarer le programme

Un programme console de Nkentseu se déclare dans un fichier `.jenga`. Il dit quel module il utilise : ici NKPlatform, plus NKContainers pour afficher du texte avec `NkPrintln`.

```jenga
with project("MonPremierProgramme"):
    consoleapp()
    language("C++")
    cppdialect("C++20")
    files(["main.cpp"])
    nkentseudependson(["NKPlatform", "NKContainers", "NKMemory", "NKCore"])
```

C'est l'essentiel. Le fichier complet qui construit les exemples de cette page, avec le choix de la chaîne de compilation par système, est `Docs/exemples/NkDocExemples.jenga`.

## Étape 2 : connaître le sens de ta machine

Une machine range les octets d'un entier dans un sens : du plus petit au plus grand (petit-boutiste, presque tous les processeurs actuels), ou l'inverse. [[NkIsLittleEndian]] te le dit, sans rien mesurer à l'exécution :

```cpp
// Le sens de la machine est connu dès la compilation : aucun test n'est fait à l'exécution.
const bool petitBoutiste = platform::NkIsLittleEndian();
NkPrintln("Cette machine est {}.", petitBoutiste ? "petit-boutiste" : "gros-boutiste");
```


## Étape 3 : inverser les octets

[[NkByteSwap32]] inverse les quatre octets d'un entier de 32 bits. C'est l'outil de base pour lire un fichier écrit par une machine de l'autre sens.

```cpp
// Inverser les octets d'un entier de 32 bits : 11 22 33 44 devient 44 33 22 11.
const uint32_t valeur = 0x11223344u;
const uint32_t inverse = platform::NkByteSwap32(valeur);
NkPrintln("0x{:08X} inversé donne 0x{:08X}", valeur, inverse);
```


## Étape 4 : parler au réseau

Sur le réseau, l'ordre est gros-boutiste. Avant d'envoyer, [[NkHostToNetwork32]] ; en recevant, [[NkNetworkToHost32]]. Sur une machine gros-boutiste, ces deux fonctions ne font rien : ton code reste le même partout.

```cpp
// L'ordre du réseau est gros-boutiste. HostToNetwork n'inverse que si la machine est petit-boutiste,
// et NetworkToHost défait exactement ce que HostToNetwork a fait.
const uint32_t envoye = platform::NkHostToNetwork32(valeur);
const uint32_t recu = platform::NkNetworkToHost32(envoye);
NkPrintln("Envoyé sur le réseau : 0x{:08X}, relu : 0x{:08X}", envoye, recu);
```


Le programme complet, avec ce qu'il affiche :

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


## Étape 5 : l'environnement du processus

Un programme reçoit des variables d'environnement de celui qui le lance. [[GetEnvVar]] en lit une ; une variable absente rend `nullptr`, pas une chaîne vide :

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


> [!attention] Teste le pointeur
> Lire un `nullptr` plante ton programme. La sortie ci-dessus montre les deux cas : une variable présente, une absente.

## Et maintenant

- Lis [NKPlatform : savoir sur quoi tu tournes](@/manuel/foundation/nkplatform) pour les pièges de la détection.
- Cherche une fonction dans la [référence de NKPlatform](@/reference/foundation/nkplatform) (raccourci : `Ctrl` `K`).
