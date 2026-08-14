# Conventions de code — Nkentseu

> **Statut : obligatoire.** À lire avant toute contribution, par un humain
> comme par un agent.
>
> **Référence normative : les modules `Kernel/Foundation/` et `Kernel/System/`.**
> Ils sont considérés conformes. Sur un point non traité ici, ouvrir
> `Kernel/System/NKThreading/src/NKThreading/NkMutex.h` ou
> `Kernel/Foundation/NKContainers/src/NKContainers/Sequential/NkVector.h` et
> faire comme eux — **pas** comme le fichier qu'on modifie, qui peut être
> antérieur à ces règles.
>
> **Portée : tout le dépôt**, moteur comme applications (NKCode, NkAnimaEditor,
> outils).
>
> ℹ️ Le guide détaillé de **nommage et documentation**
> (`NOMENCLATURE_ET_DOCUMENTATION.md`) vit dans `Pcp/`, qui est **exclu du dépôt**
> par `.gitignore`. Il n'est donc pas visible depuis un clone : ce fichier-ci
> rappelle l'essentiel pour rester utilisable sans lui.

> ### ⚠️ SI TU LIS CECI DEPUIS UN WORKTREE, IL TE MANQUE LES RÈGLES (2026-08-14)
>
> Ce fichier est **suivi par git**, donc présent partout. **Ce n'est pas le cas
> des documents qui portent les règles et les décisions d'architecture** — ils
> sont dans le `.gitignore`, donc **absents de tout worktree neuf**, et rien ne
> signale leur absence : le dépôt a l'air complet.
>
> Manquent notamment `CLAUDE.md` (**règles git multi-agents, conventions, et
> TOUS les blocs de décision** — substrat de graphe unique NKGraph, substrats
> animation/comportement, exclusivité NKCanvas/NKRenderer),
> `GUIDE_GIT_MULTI_AGENTS.md`, `ETAT_TRAVAUX.md`, `PLATEFORMES_ETAT.md`,
> `PRINCIPES_CONCEPTION.private.md` et `Engine/Noge/CONTINUATION.md` — ce dernier
> listant les fichiers **VERROUILLÉS** par un autre agent.
>
> **Parade, à faire avant d'écrire une ligne de code :**
> ```
> cp ../Nkentseu/CLAUDE.md .
> cp ../Nkentseu/GUIDE_GIT_MULTI_AGENTS.md .
> ```
> (adapter si le dossier principal n'est pas `../Nkentseu`).
>
> **Pourquoi ça compte :** l'audit du 14/08 a trouvé un fork de 5 211 lignes du
> lecteur PDF, deux `NkSWPixel`, et un **troisième** sélecteur de dossier écrit un
> mois après la règle qui l'interdisait. Une règle absente du répertoire où l'on
> travaille n'est pas une règle. Détail complet et question de fond — faut-il
> verser ces documents au suivi git ? — dans `GUIDE_GIT_MULTI_AGENTS.md` § 8bis,
> **non tranchée**, arbitrage de Rodolf.

---

## 1. Une instruction par ligne

Une ligne porte **une seule instruction**. Pas de point-virgule multiple, pas de
corps de condition sur la ligne du `if`.

```cpp
// CONFORME
int32 width = 0;
int32 height = 0;
if (!ready) {
    return false;
}

// NON CONFORME
int32 width = 0; int32 height = 0;
if (!ready) { return false; }
if (!ready) return false;
```

**Pourquoi**, par ordre d'importance :

1. **Le débogueur travaille par ligne.** Sur `if (!ready) return false;`, aucun
   point d'arrêt ne permet de s'arrêter *avant* le `return`. On perd l'outil au
   moment précis où il servirait.
2. **Le diff redevient lisible.** Modifier la seconde instruction d'une ligne
   double fait apparaître la première comme modifiée : la revue porte sur du
   bruit.
3. **La couverture de code devient exploitable.** Une ligne à deux instructions
   compte comme couverte dès que la première s'exécute.

**Exception unique et bornée** : une table d'aiguillage dont chaque cas tient en
un appel, où l'alignement porte le sens mieux que la version éclatée.

```cpp
// TOLERE
switch (kind) {
    case NkPathVerb::Move:  out.MoveTo(x, y);  break;
    case NkPathVerb::Line:  out.LineTo(x, y);  break;
    case NkPathVerb::Close: out.Close();       break;
}
```

Dès qu'un cas demande deux instructions ou une condition, la table s'éclate.

---

## 2. Indentation stricte des commentaires

Un commentaire s'indente **exactement comme la ligne qu'il décrit**. Jamais en
colonne 0 dans un bloc, jamais décalé d'un cran.

```cpp
// CONFORME
void NkCanvas::FillPath(const NkPath &path, bool evenOdd) {
    // La couverture est bornée à la boîte du tracé : remettre à zéro un tampon
    // de la taille de la page pour chaque tracé coûtait des giga-octets.
    int32 x0 = 0;
    int32 y0 = 0;

    for (int32 y = y0; y < y1; ++y) {
        // Les pixels hors boîte gardent leur valeur : c'est tout l'intérêt.
        if (!cov[y]) {
            continue;
        }
    }
}

// NON CONFORME
void NkCanvas::FillPath(const NkPath &path, bool evenOdd) {
// Commentaire en colonne 0 : se lit comme appartenant à un autre bloc.
    int32 x0 = 0;
        // Décalé d'un cran sans raison.
    int32 y0 = 0;
}
```

**Pourquoi.** Un commentaire désaligné se lit comme appartenant à une autre
portée. Sur une fonction longue, il fait croire à une structure qui n'existe
pas — et c'est ainsi qu'on finit par modifier la mauvaise branche.

### 2.1 Commentaire de fin de ligne

Séparé du code par **deux espaces au minimum**. On ne cherche **pas** à aligner
verticalement plusieurs commentaires consécutifs : le premier renommage de
variable détruit l'alignement et produit un diff inutile.

```cpp
// CONFORME
int32 timeout = 5000;  // délai réseau typique
NkString name;         // alignement fortuit, non contraignant

// NON CONFORME — alignement forcé, à maintenir à la main
int32 timeout      = 5000;   // délai
NkString name;               // nom
```

### 2.2 Commentaire de bloc

Il précède le bloc, à son indentation, sans ligne vide qui le détacherait de ce
qu'il décrit.

```cpp
    // ── Liste d'arêtes actives ──
    // Sans elle, chaque sous-ligne reparcourt toutes les arêtes de la page.
    NkVector<int32> active;
```

---

## 3. Fond des commentaires

Un commentaire dit **pourquoi**, jamais **quoi** : le code dit déjà le quoi.

```cpp
// UTILE — explique une décision non évidente
mutex.Lock();  // requis : accès depuis le thread de rendu

// INUTILE — répète le code
int32 timeout = 5000;  // assigne 5000 à timeout
```

Sont particulièrement attendus : les **pièges** rencontrés, les **mesures** qui
justifient un choix, et les **limites assumées**. Un commentaire qui a coûté une
heure de diagnostic vaut d'être écrit.

---

## 4. En-tête de fichier

Gabarit des modules `System`, à reprendre tel quel :

```cpp
//
// NkMutex.h
// =============================================================================
// Description :
//   Ce que fait le fichier, en une ou deux phrases.
//
// Caractéristiques :
//   - Points saillants de l'implémentation
//
// Algorithmes implémentés :
//   - Ceux qui méritent d'être nommés
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_<MODULE>_<FICHIER>_H__
#define __NKENTSEU_<MODULE>_<FICHIER>_H__
```

Le `#pragma once` **et** la garde nommée : les deux, pas l'un ou l'autre.

---

## 5. Nommage — rappel

| Élément | Forme | Exemple |
|---|---|---|
| Classe, structure, énumération | `PascalCase` préfixé `Nk` | `NkPdfCanvas` |
| Méthode publique | `PascalCase` | `FillPath()` |
| Méthode privée | `PascalCase` | `Rasterize()` |
| Membre | `mPascalCase` | `mClipStack` |
| Variable locale | `camelCase` | `edgeCount` |
| Constante, macro | `UPPER_SNAKE_CASE` | `NK_PDF_STREAM` |
| Fonction DSL utilisateur (Jenga) | `minuscules` | `consoleapp()` |

**Pas de `snake_case`** pour les fonctions et les types, en C++ comme en Python.

---

## 6. Application

**Nouveau fichier** : conforme dès le premier commit, sans exception.

**Fichier existant** : mis en conformité **sur les parties qu'on touche
réellement**. On ne reformate pas un fichier entier au passage — un diff de
800 lignes dont 5 sont du vrai changement ne peut pas être relu, et c'est ainsi
qu'un défaut passe inaperçu.

**Revue** : un manquement à ces règles est un motif de retour légitime, au même
titre qu'un nom mal choisi.
