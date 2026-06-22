# Extensions — packages & plugins externes (façon VSCode)

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Extensions` rend NKCode **ouvert et extensible**, comme VSCode l'est avec ses extensions. Sans lui,
NKCode ne ferait que ce qu'on a codé en dur ; avec lui, **n'importe qui peut ajouter** des
capacités — sans modifier le cœur. C'est ce qui transforme un éditeur en **plateforme**.

Il couvre **deux sens** complémentaires de « package externe » :

1. **Extensions d'éditeur** — des plugins qui **étendent NKCode lui-même** : nouvelles **commandes**,
   **panneaux**, **langages** (coloration/complétion), **thèmes**, et surtout de nouveaux **nœuds**
   pour [Blueprint](../Blueprint/README.md)/[Blocks](../Blocks/README.md). Chargées comme **plugins
   natifs** (bibliothèques dynamiques) et/ou **extensions scriptées** (un runtime embarqué — Python
   est le choix naturel puisque Jenga est en Python, comme VSCode s'appuie sur JS).
2. **Packages de projet** — des **dépendances** pour les projets de l'utilisateur (bibliothèques,
   toolchains, modèles de projet), **gérées via le packaging de Jenga** (publish/install ;
   vcpkg/conan/npm sont sur la roadmap de Jenga). NKCode = le front-end ; Jenga = le gestionnaire.

Le mécanisme central est un **manifeste** (ce que l'extension contribue) + des **points de
contribution** (les hooks où s'accrocher) + un **chargeur** (découverte, activation, isolation). On
commence **local** (un dossier d'extensions), puis on grandit vers un **registre / marketplace**.

## Responsabilités
- **Manifeste** d'extension + **points de contribution** (commandes, panneaux, langages, nœuds, thèmes).
- **Chargeur** : découverte, activation/désactivation, cycle de vie ; natif (DLL) + scripté (Python).
- **API d'extension** stable (façon `vscode.*`) + isolation/sandboxing (sécurité).
- **Packages de projet** : recherche/installation de dépendances **via Jenga** ; vue « extensions ».

## Dépend de
NKFileSystem, NKPlatform (chargement dynamique), **NKReflection** (découverte/enregistrement),
**Jenga** (packages de projet), le Shell (contributions UI).

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
