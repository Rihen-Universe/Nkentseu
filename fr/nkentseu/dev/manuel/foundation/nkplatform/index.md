# NKPlatform : savoir sur quoi tu tournes

Le module le plus bas du moteur. Il dit, dès la compilation, pour quel système, quel processeur et quel compilateur tu construis ; à l'exécution, ce que le processeur sait faire et ce que contient l'environnement.


## À quoi il sert

NKPlatform est la fondation de toutes les autres couches : il ne dépend de rien, et presque tout le moteur dépend de lui. Il répond à trois questions.

- **Pour quoi est-ce que je compile ?** Des macros te disent le système (`NKENTSEU_PLATFORM_WINDOWS`, `NKENTSEU_PLATFORM_LINUX`, `NKENTSEU_PLATFORM_ANDROID`...), l'architecture ([[NKENTSEU_ARCH_64BIT]]) et le compilateur (`NKENTSEU_COMPILER_CLANG`, `NKENTSEU_COMPILER_MSVC`...). Elles sont fixées par le préprocesseur : elles décrivent la machine **cible**, pas celle qui lance le programme.
- **Comment ranger les octets ?** [[NkIsLittleEndian]] te dit le sens de la machine, [[NkByteSwap32]] inverse les octets d'un entier, [[NkHostToNetwork32]] et [[NkNetworkToHost32]] passent à l'ordre du réseau et en reviennent.
- **Qu'est-ce que la machine sait faire, ici et maintenant ?** [[NkGetCPUFeatures]] lit le processeur à l'exécution (SSE2, AVX, NEON, taille des lignes de cache, nombre de cœurs). [[GetEnvVar]] lit une variable d'environnement du processus.

## Quand l'utiliser

- Tu écris du code qui doit se comporter différemment selon le système : teste une macro `NKENTSEU_PLATFORM_*`, jamais `_WIN32` ou `__linux__` directement. Le jour où une plateforme s'ajoute, une seule détection change.
- Tu écris ou lis un format binaire (fichier, réseau) : fixe l'ordre des octets du format, et convertis avec les fonctions de `NkEndianness.h`.
- Tu veux choisir à l'exécution le chemin le plus rapide (une boucle vectorisée, par exemple) : demande à [[NkGetCPUFeatures]], pas au compilateur.
- Tu lis un réglage passé par l'environnement (un chemin, un mode de banc) : `NkEnv.h`.
- Tu écris un module de Foundation, plus bas que NKLogger : les macros `NK_FOUNDATION_LOG_*` journalisent, et [[NkFoundationSetLogSink]] envoie les messages où tu veux.

Pour le lier, une ligne dans ton `.jenga` :

```jenga
nkentseudependson(["NKPlatform"])
```

`#include "NKPlatform/NKPlatform.h"` donne d'un coup la détection, l'ordre des octets, les macros d'export et d'inlining. `NkEnv.h` et `NkCPUFeatures.h` s'incluent à part.

## Un exemple complet

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


## Compilation, puis exécution

Les macros répondent à la compilation :

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


Le processeur répond à l'exécution :

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


## Les pièges

> [!attention] Compilation contre exécution
> Les macros de détection sont figées au moment de la compilation. Un programme compilé pour x86_64 et lancé sous émulation sur un processeur ARM dit toujours x86_64. Pour savoir ce que le processeur sait faire vraiment, c'est [[NkGetCPUFeatures]], à l'exécution.

> [!attention] Une variable absente rend nullptr
> [[GetEnvVar]] rend un pointeur vers la valeur, ou `nullptr` si la variable n'existe pas. Teste toujours le pointeur avant de le lire.


> [!attention] Une variable d'environnement vit dans le processus
> [[NkSet]] modifie l'environnement de TON processus (et des processus qu'il lancera ensuite). Il ne change rien pour la session de l'utilisateur, et tout disparaît quand ton programme s'arrête.

> [!attention] Une variable absente et une variable vide se ressemblent
> [[NkGet]] rend une chaîne vide dans les deux cas. Si la différence compte, demande d'abord à [[NkExists]].

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


> [!attention] Les macros de X11
> Les en-têtes de X11 définissent des macros aux noms très courants (`None`, `Bool`, `Status`...). Si tu inclus X11 avant un en-tête de Nkentseu qui utilise ces noms, le préprocesseur les remplace. `NkX11Clean.h` les retire : inclus-le juste après X11.

> [!pourquoi] Pourquoi une couche rien que pour ça
> Tout le reste du moteur en dépend : si chaque module détectait lui-même le système, il y aurait autant de façons de se tromper que de modules. Une seule détection, en bas de la pile, et tout le monde la lit.

## Pour aller plus loin

- Le tutoriel [Ton premier programme avec NKPlatform](@/apprendre/nkplatform-premiers-pas) construit et lance deux petits programmes.
- La [référence de NKPlatform](@/reference/foundation/nkplatform) liste chaque fonction, chaque type et chaque macro, avec sa signature.
