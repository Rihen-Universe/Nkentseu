# Écrire une IA

Le fichier de ce chapitre :

**`Applications/ConquerorLab/exemples/ai/IAMinimale.cpp`**

Il tient en une idée : **demander les coups légaux au moteur, en prendre un au
hasard.** C'est nul, et c'est le but.

## 3.1 Pourquoi commencer par une IA nulle

Une IA aléatoire est le **plancher de mesure**. Sans elle, vous n'avez aucun
point de comparaison : une IA qui gagne 70 % ne veut rien dire tant qu'on ne sait
pas contre quoi.

Le premier run du banc d'essai livre déjà un enseignement de cette nature :

**`Applications/ConquerorLab/README.md — section « État vérifié »`**

```
Le glouton bat l'aléatoire 29–6. La position se juge, l'évaluation
matérielle mord.
```

C'est une information réelle sur le jeu, obtenue avec deux IA triviales. Une IA
qui ne bat pas l'aléatoire ne vaut rien ; une qui la bat 55 % du temps ne vaut
pas beaucoup plus.

## 3.2 Le point de conception central

**`Applications/ConquerorLab/include/Conqueror/ConquerorAIABI.h:14-22`**

```
POINT DE CONCEPTION CENTRAL
L'IA ne connait PAS l'implementation du moteur de regles : elle recoit une
NkcRulesVTable et un handle opaque. Consequences voulues :

  1. Le sujet A2 demarre en semaine 1 sur un module bouchon, sans attendre A1.
  2. Quand A1 change son code interne, l'IA continue de fonctionner.
  3. Les deux ne peuvent pas diverger sur les regles : il n'en existe qu'une
     implementation, et l'IA la traverse.
```

Concrètement, votre IA ne sait pas ce qu'est une duplication. Elle ne connaît ni
le plateau, ni la topologie, ni les paramètres. Elle demande :

**`Applications/ConquerorLab/exemples/ai/IAMinimale.cpp — V_ChooseMove`**

```cpp
// TOUT passe par la table : on ne sait pas ce qu'on joue, on demande.
NkcMove      moves[kMoveCap];
const uint32 total = R->GenerateLegalMoves(ri, st, moves, kMoveCap);
const uint32 n     = total < kMoveCap ? total : kMoveCap;
if (n == 0) return 0;
```

> **✅ Ce qu'il faut retenir**
>
> Vous pouvez écrire une IA **avant** que les règles existent. Vous pouvez la
> tester contre trois moteurs différents sans la recompiler. Et le jour où A1
> change sa représentation interne, vous ne le saurez même pas.

## 3.3 La neuf-fonctions

`NkcAIVTable` est beaucoup plus courte que celle des règles :

| Fonction | Obligatoire ? | Rôle |
|---|---|---|
| `Create` / `Destroy` | oui | cycle de vie |
| `Configure` | oui | palier, budget, graine — appelée avant le premier `ChooseMove` |
| `ChooseMove` | oui | **le cœur** |
| `GetParamsSchemaJson` / `SetParam` | non | réglages exposés dans l'interface ; renvoyer `""` est légal |
| `OnMovePlayed` | non | un coup a réellement été joué (réutilisation d'arbre) |
| `Reset` | non | nouvelle partie : vider la mémoire inter-coups |
| `GetDebugJson` | non | ce que l'atelier pourra afficher plus tard |

## 3.4 `ChooseMove`, ligne à ligne

```cpp
int32 V_ChooseMove(NkcAI self, const NkcRulesVTable *R, NkcRules ri,
                   const NkcState st, NkcAIResult *out) {
    AI *a = static_cast<AI *>(self);
    if (!a || !R || !ri || !st || !out) return 0;

    std::memset(out, 0, sizeof(*out));
    out->move.targetLevel = -1;
    out->move.powerId     = -1;
```

> **⚠️ Le même piège que pour les règles**
>
> `NkcMove` se compare **octet à octet**. Un champ laissé au hasard rend votre
> coup illégal aux yeux du moteur. Ici on remet tout à zéro, puis on repose les
> deux champs « aucun » (`-1`).
>
> En pratique ce n'est pas strictement nécessaire quand on recopie un coup issu
> de `GenerateLegalMoves` (il est déjà propre) — mais une IA qui *construit* un
> coup au lieu de le choisir doit absolument le faire.

```cpp
    const uint32 pick = static_cast<uint32>(NextRand(a->rng) % n);
    out->move         = moves[pick];
    out->simulations  = 1;
    out->depthReached = 0;
    out->scoreMilli   = 0;
    return 1;
}
```

Le retour est binaire : **`1` = j'ai un coup, `0` = je n'en ai aucun**. Dans le
second cas l'atelier joue PASSER lui-même.

### Le compte-rendu

`NkcAIResult` n'est pas décoratif : le panneau *Joueurs* l'affiche après chaque
réflexion.

**`Applications/ConquerorLab/include/Conqueror/ConquerorAIABI.h:78-86`**

```cpp
struct NkcAIResult {
        NkcMove move;
        int32   scoreMilli   = 0;  ///< -1000 = perdue, +1000 = gagnee
        uint32  simulations  = 0;  ///< simulations/noeuds reellement faits
        uint32  elapsedMs    = 0;
        uint32  depthReached = 0;
        uint8   hitBudget    = 0;  ///< 1 si coupee par le budget
        uint8   _pad[3]      = {};
};
```

> **✅ `scoreMilli` est en MILLIÈMES**
>
> Aucun flottant à la frontière, même pour un score. Même raison qu'ailleurs :
> deux plateformes doivent produire le même nombre.

## 3.5 Le PRNG : porté par l'instance

**`Applications/ConquerorLab/exemples/ai/IAMinimale.cpp — NextRand`**

```cpp
/// PRNG xorshift64* PORTE PAR L'INSTANCE, jamais global. Sans cela, deux
/// instances d'IA jouant en parallele dans une campagne piochent dans le
/// meme flux, et deux parties de meme graine cessent d'etre reproductibles.
uint64 NextRand(uint64 &s) {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 2685821657736338717ull;
}
```

La graine arrive par `Configure` :

```cpp
void V_Configure(NkcAI self, const NkcAIConfig *cfg) {
    AI *a = static_cast<AI *>(self);
    if (!a || !cfg) return;
    a->cfg = *cfg;
    a->rng = cfg->seed ? cfg->seed : 0x243F6A8885A308D3ull;
}
```

Et l'atelier la dérive de `(partie, tour, joueur)` :

**`Applications/ConquerorLab/src/ConquerorLab/NkcSession.h — StartThinking`**

```cpp
// Graine derivee de (partie, tour, joueur) : deux parties de meme graine
// rejouent a l'identique, et deux tours ne partagent pas le meme flux.
c.seed = mSeed ^ (static_cast<uint64>(mView.turn) * 0x9E3779B97F4A7C15ull) ^
         (static_cast<uint64>(mView.current) + 1ull);
```

> **⚠️ Un `static` dans votre IA, et la campagne ment**
>
> Une campagne fait tourner **plusieurs instances de votre IA en parallèle**, une
> par worker. Un compteur `static`, un cache global, un générateur partagé : et
> vos parties cessent d'être indépendantes. Le résultat reste plausible — c'est
> le pire des cas, parce que vous ne le verrez pas.
>
> Déclarez-le franchement dans votre fabrique : `info.isThreadSafe = 0` si vous
> n'en êtes pas sûr.

## 3.6 Le thread, et le budget

**`Applications/ConquerorLab/include/Conqueror/ConquerorAIABI.h:24-32`**

```
CONTRAINTE DE THREAD (SPIKE Feature #4)
ChooseMove est appelee depuis un THREAD WORKER, jamais depuis le thread de
rendu. Elle doit :
  - ne toucher que l'etat qu'on lui passe (c'est un CLONE prive) ;
  - ne poser aucun verrou sur une ressource partagee ;
  - respecter `budgetMs` : au-dela, elle rend le meilleur coup trouve
    jusque-la. Depasser le budget fige l'interface — c'est un echec.
```

Côté atelier, l'isolement est total :

**`Applications/ConquerorLab/src/ConquerorLab/NkcSession.h — en-tête, point 2`**

```
L'IA REFLECHIT SUR SON PROPRE MOTEUR. Plutot que de partager l'instance de
regles avec le thread de rendu — ou l'utilisateur peut a tout moment bouger un
parametre — le thread possede SA PROPRE instance, synchronisee avant chaque
reflexion (plateau + parametres), et son propre etat, transfere par
SerializeState/DeserializeState. Zero verrou, zero course.
```

Autrement dit : l'état que vous recevez est **à vous**. Vous pouvez le cloner
autant que vous voulez. Mais vous ne devez **pas** le muter directement — vous
passez par `rules->ApplyMove` sur vos clones.

### Respecter le budget

`IAMinimale` répond instantanément, la question ne se pose pas. Dès que vous
faites une recherche, elle se pose à chaque nœud. Le motif est celui-ci :

**`Idiome — écrit pour ce cours, d'après ConquerorAIABI.h:113-115`**

```cpp
// Au debut de ChooseMove : noter l'instant de depart (l'IA choisit sa
// source de temps ; le contrat n'en impose aucune).
// Dans la boucle de recherche, tous les N noeuds :
if (a->cfg.budgetMs != 0 && ElapsedMs(start) >= a->cfg.budgetMs) {
    out->hitBudget = 1;
    break;                       // on rend le MEILLEUR coup trouve jusque-la
}
```

Deux règles :

- **`budgetMs == 0` signifie illimité.** Ne le traitez pas comme « zéro
  milliseconde ».
- **Ne testez pas le temps à chaque nœud.** Un appel horloge par nœud coûte plus
  cher que le nœud. Tous les 1024 nœuds suffit.

> **⚠️ Ce que « figer l'interface » veut dire concrètement**
>
> Le panneau *Joueurs* affiche `hitBudget` sous la forme « coupée par le budget ».
> Mais surtout : quand vous cliquez *Nouvelle partie* pendant une réflexion,
> l'atelier **attend** que votre `ChooseMove` rende la main. Une IA qui ignore
> son budget rend le bouton mou pendant tout ce temps.

## 3.7 Les paliers de difficulté

**`Applications/ConquerorLab/include/Conqueror/ConquerorAIABI.h:41-51`**

```cpp
/// Cinq paliers (fiche A2 §4.1.3). Reperes, pas implementation imposee :
/// c'est au module de decider comment il module sa force (budget de
/// simulations, profondeur, bruit, erreurs volontaires).
enum class NkcDifficulty : uint8 {
    Easy = 0, Normal = 1, Hard = 2, Expert = 3, Apex = 4, Count = 5
};
```

L'IA de référence les utilise comme trois stratégies distinctes :

**`Applications/ConquerorLab/modules/ai/ConquerorAIRef.cpp:3-8`**

```
Trois strategies dans un module, choisies par le palier de difficulte :
  Facile           : aleatoire pur
  Normal           : glouton (maximise le gain immediat)
  Hard/Expert/Apex : Minimax alpha-beta, profondeur 2 / 3 / 4
```

Si votre IA ne distingue qu'un seul niveau, dites-le :
`info.difficultyCount = 1`. L'atelier affichera quand même les cinq paliers —
c'est vous qui savez ce qu'ils changent.

## 3.8 Évaluer une position sans connaître les règles

C'est le problème intéressant du sujet A2. Voici comment l'IA de référence s'y
prend, et c'est exactement ce qu'il faut remettre en cause :

**`Applications/ConquerorLab/modules/ai/ConquerorAIRef.cpp — Evaluate`**

```cpp
int32 mine = 0, theirs = 0;
for (uint8 p = 0; p < v.playerCount; ++p) {
    if (p == me) mine += v.totemCount[p];
    else         theirs += v.totemCount[p];
}

// Mobilite du joueur au trait, bornee pour rester bon marche.
NkcMove      buf[64];
const uint32 n        = R->GenerateLegalMoves(ri, st, buf, 64);
int32        mobility = static_cast<int32>(n > 64 ? 64 : n);
if (v.current != me) mobility = -mobility;

return ai->wMaterial * (mine - theirs) + ai->wMobility * mobility;
```

Deux termes seulement — le **matériel** et la **mobilité** — et deux poids
réglables. Tout ce qu'une IA peut savoir vient de `NkcStateView` et de
`GenerateLegalMoves` : nombre de totems, niveaux, ressources, nombre de coups
disponibles, et ce que devient la position après un coup simulé.

> **✅ Le vrai sujet de A2**
>
> Cette évaluation est délibérément grossière. « Le glouton bat l'aléatoire
> 29–6 » dit que le matériel mord ; il ne dit pas que c'est le bon critère. Une
> position avec beaucoup de totems mais peu de mobilité est-elle bonne ? Un
> totem isolé au centre vaut-il un totem en bord de plateau ? **Ce sont des
> questions que seule la mesure tranche** — chapitre 5.

## 3.9 `OnMovePlayed` : ce qui distingue un MCTS d'une recherche jetable

```cpp
/// Un coup a REELLEMENT ete joue en partie. Une IA a arbre (MCTS) s'en sert
/// pour reutiliser son travail d'un tour a l'autre. Ici : rien a faire.
void V_OnMovePlayed(NkcAI, const NkcMove *) {}
```

Une recherche naïve jette tout son travail à chaque tour. Un MCTS conserve son
arbre : quand un coup est joué, il **descend** dans le sous-arbre correspondant et
repart de là. `OnMovePlayed` est le signal qui le lui permet, et `Reset` celui qui
lui dit de tout jeter (nouvelle partie).

> **⚠️ Attention en campagne**
>
> Une campagne réutilise la même instance d'IA d'une partie à l'autre. Sans
> `Reset` correct, l'arbre de la partie précédente contamine la suivante — et vos
> mesures deviennent dépendantes de l'ordre des parties. Le lanceur de campagne
> appelle `Configure` puis `Reset` au début de chaque partie ; à vous d'honorer le
> second.

## 3.10 Votre première IA, en quatre gestes

1. Copiez `exemples/ai/IAMinimale.cpp` vers `Build/ConquerorLab/ai/mon_ia.cpp`.
2. Changez le nom dans `FillFactory`.
3. Sauvegardez, attendez une seconde, ouvrez le panneau *Joueurs* : votre IA est
   dans la liste des pilotes.
4. Mettez-la sur le siège 1, l'IA de référence sur le siège 0, et lancez une
   campagne de 200 parties (panneau *Métriques*).

Vous venez de mesurer votre plancher. Tout ce que vous ferez ensuite se compare
à ce chiffre.

## Exercices

> **✏️ 1 — Le glouton**
>
> Transformez `IAMinimale` en glouton : pour chaque coup légal, clonez l'état,
> appliquez le coup, comptez vos totems, gardez le meilleur. Vous aurez besoin de
> `R->CreateState`, `R->CloneState`, `R->ApplyMove`, `R->GetView`,
> `R->DestroyState`. Mesurez-le contre l'aléatoire sur 200 parties. Combien
> obtenez-vous ?

> **✏️ 2 — Le poids réglable**
>
> Exposez un paramètre `poids_mobilite` via `GetParamsSchemaJson` (même format
> que pour les règles, chapitre 2.3). Vérifiez qu'il apparaît bien dans
> l'interface. Puis faites varier ce poids et mesurez : y a-t-il une valeur
> optimale, ou le résultat est-il plat ?

> **✏️ 3 — Le budget**
>
> Ajoutez une recherche à profondeur 3, puis un test de budget tous les 1024
> nœuds. Réglez le budget à 5 ms dans le panneau *Joueurs* et vérifiez que
> `hitBudget` passe bien à 1. Que se passe-t-il si vous oubliez le test ?

> **✏️ 4 — Le piège du `static`**
>
> Ajoutez un `static int compteur = 0;` dans votre `ChooseMove` et faites-le
> intervenir dans le choix du coup. Lancez une campagne sur 8 threads, deux fois,
> avec la même graine. Les résultats sont-ils identiques ? Expliquez.
