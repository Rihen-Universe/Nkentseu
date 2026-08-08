# Se servir de l'atelier

Les chapitres précédents produisaient du code. Celui-ci apprend à s'en servir —
et surtout à **lire un résultat sans se tromper**, ce qui est plus difficile que
de l'obtenir.

## 6.1 Les sept panneaux

Tous sont dockables : on les déplace, on les empile en onglets, on les ferme.

| Panneau | Ce qu'il fait |
|---|---|
| **Plateau** | l'état, le picking, les coups légaux, la menace, le dernier coup, le score |
| **Règles** | la grille + tous les paramètres du moteur, auto-générés |
| **Joueurs** | qui tient chaque siège, à quelle force, avec quel budget |
| **Modules** | les `.cpp` détectés, leur statut, **la sortie du compilateur** |
| **Journal** | la suite des coups, leur empreinte, le curseur de rejeu |
| **Métriques** | la campagne IA contre IA, et ce qu'elle répond |
| **Sortie** | **ce que votre code écrit** quand il tourne (`NKC_LOG_*`) |

> **⚠️ Un panneau qui n'est pas l'onglet actif ne se dessine pas**
>
> C'est le fonctionnement normal du docking. Conséquence : ouvrez le panneau
> *Métriques* pour voir la progression d'une campagne. La campagne, elle, tourne
> quand même — elle vit sur ses propres threads, pas dans l'interface.

## 6.2 Lire le plateau

- **Disques colorés** : les totems. Le **niveau** se lit à la *taille* et au
  liseré, jamais à la seule couleur — celle-ci porte déjà le propriétaire.
- **Anneaux verts** : les destinations légales du totem sélectionné.
- **Anneaux rouges** : les totems ennemis que le coup **survolé** retournerait.
- **Anneau orange qui pulse** : le dernier coup joué, une seconde environ.
- **« CASCADE ×N »** au centre : deux totems ou plus retournés d'un coup.

Les anneaux rouges sont la lecture centrale du jeu : *« quelle surface de contact
suis-je en train d'offrir ? »*. Ils ne sont pas déduits des règles — l'atelier
simule le coup et lit les événements. Ils restent donc justes même quand vous
changez la règle de transformation.

### La barre d'état

Elle dit à tout instant ce que l'atelier attend. Relisez-la avant de chercher un
bug : *au tour du joueur humain*, *l'IA réfléchit*, *en pause*, *rejeu en pause*,
*partie terminée*, *IA introuvable*.

### Les quatre boutons

| Bouton | Effet |
|---|---|
| **Nouvelle partie** | rejoue depuis le début, mêmes réglages, même graine |
| **Lecture / Pause** | la partie avance-t-elle toute seule ? |
| **Pas à pas** | un seul coup d'IA, même en pause |
| **IA vs IA** | tous les sièges à l'IA + lecture — la configuration de mesure |
| **Siège : Humain / IA** | bascule le siège **au trait** |
| **Voisinage** | trace les liens d'adjacence de la case survolée (mise au point) |

## 6.3 Le panneau Sortie : vos propres traces

Le panneau **Modules** montre ce que dit le *compilateur* ; celui-ci montre ce
que dit votre *code* une fois qu'il tourne. Les deux sont nécessaires, et les
mélanger rendrait les deux illisibles.

- **Niveau minimal** filtre : passez à `WARN` quand vous cherchez une anomalie
  dans un flot de `INFO` ;
- les lignes identiques **consécutives** sont fusionnées en `(xN)` — une boucle
  qui répète la même trace mille fois reste lisible ;
- le plus récent est **en haut**, donc toujours visible sans défiler ;
- le tampon est borné à **4096 lignes**, et le panneau **affiche ce qu'il a
  perdu**. Un journal qui perd des lignes en silence fait chercher un bug qui
  n'existe pas.

> **⚠️ Les modules internes n'y écrivent pas**
>
> `ConquerorRulesV2 (interne)` et `AIRef (interne)` sont compilés *dans*
> l'application : ils partagent son journal, et leurs traces vont dans la console,
> pas ici. Le panneau Sortie est là pour **votre** code.

## 6.4 Reproduire un bug : le journal

C'est la fonction la plus utile de l'atelier, et la moins spectaculaire.

Le panneau *Journal* liste les coups avec, à droite de chacun, **l'empreinte de
l'état après ce coup**. Le bouton **Copier** met dans le presse-papiers une trace
rejouable :

**`Applications/ConquerorLab/src/ConquerorLab/NkcJournalPanel.h — CopyJournal`**

```cpp
/// Copie une trace REJOUABLE : graine, nombre de joueurs, empreinte
/// finale, puis un coup par ligne. C'est le format qu'on colle dans
/// un rapport de bug.
```

> **✅ Comment on s'en sert vraiment**
>
> Votre moteur produit un résultat différent sur Linux et sur Windows. Vous
> copiez le journal des deux côtés, vous comparez les colonnes d'empreintes, et
> vous trouvez **la première ligne qui diffère**. Ce coup-là, et pas un autre,
> contient votre non-déterminisme. Sans les empreintes intermédiaires, vous
> sauriez seulement que les états finaux diffèrent — c'est-à-dire presque rien.

Le bouton **Copier** produit une trace **auto-descriptive** :

```
# ConquerorLab — trace rejouable
# graine        20260807
# joueurs       2
# regles        ConquerorRulesV2 (interne)
# siege 0       Humain
# siege 1       AIRef (1.0.0)
# plateau       hexagone_6x7.json
#
# joueur action de_q de_r vers_q vers_r empreinte
```

La graine et les coups ne suffisaient pas : rejouée avec un autre moteur ou une
autre IA, la même liste donne un autre résultat, et on cherche un bug là où il
n'y en a pas. Vous êtes deux à travailler en parallèle — sans cet entête, un
rapport ne dit même pas de qui vient le code qu'il incrimine.

Le curseur de rejeu reconstruit la position (`Setup` + N coups) au lieu de
« défaire » le dernier coup : le contrat n'impose au moteur aucune opération
inverse, et une annulation approximative donnerait un état qui n'a jamais existé.

## 6.5 Deux garde-fous contre les erreurs silencieuses

Ce sont les plus dangereuses : rien ne plante, les chiffres sortent, et ils sont
faux.

### L'incohérence voisinage / coups légaux

Un moteur déclare son voisinage (`GetNeighbors`) et génère ses coups. Rien
n'oblige les deux à s'accorder. Quand ils divergent, **rien ne se plaint** — mais
une IA qui compte « combien d'ennemis touche cette case » se trompe, et joue
simplement moins bien. Une semaine perdue à chercher dans l'évaluation un bug qui
est dans la géométrie.

L'atelier vérifie donc, à chaque nouvelle partie, qu'un coup DUPLIQUER va bien
vers une case que `GetNeighbors` déclare voisine. Sinon, bandeau rouge en tête du
panneau *Modules* :

```
INCOHERENCE : 3 coup(s) DUPLIQUER visent une case que GetNeighbors ne declare
pas voisine — p.ex. (2,1) -> (4,1). Ton generateur de coups et ton voisinage ne
disent pas la meme chose ; toute IA qui evalue l'adjacence se trompera.
```

Il ne dit pas lequel des deux a tort — il signale la contradiction, ce qui suffit
à la faire chercher.

### La signature de configuration

```
campagne du matin  : 62 % pour le camp 0
campagne du soir   : 51 % pour le camp 0
```

Que s'est-il passé ? A1 a changé un paramètre ? A2 son évaluation ? Les deux ?
Sans réponse, ces deux nombres ne veulent **rien** dire — et on ne peut même pas
savoir qu'ils ne veulent rien dire.

Le panneau *Métriques* affiche donc, **au-dessus** des chiffres, tout ce qui les
a produits : moteur, IA de chaque siège, paliers, budget, plateau, valeurs de
**tous** les paramètres, plus un identifiant court. Et il compare avec la
campagne précédente :

```
Depuis la campagne precedente, les PARAMETRES, les IA ont change.
Les deux resultats ne se comparent pas.
```

> **✅ Pourquoi ça marche là où la discipline échoue**
>
> « Une variable à la fois » est la bonne règle. Mais une discipline qu'on ne
> peut pas **vérifier après coup** n'en est pas une : personne ne se souvient,
> une semaine plus tard, de ce qui avait bougé. On n'empêche personne de changer
> deux choses ; on rend impossible de ne pas s'en apercevoir.

## 6.6 La campagne : ce qu'elle mesure, et comment

Panneau *Métriques*. Réglez le nombre de parties, les threads, le budget, les IA
de chaque camp, puis **Lancer la campagne**.

### L'inversion des côtés

**`Applications/ConquerorLab/src/ConquerorLab/NkcBatch.h — en-tête`**

```
2. LES COTES SONT INVERSEES une partie sur deux. Sans cela, tout ecart
   mesure melange « cette IA est meilleure » et « le premier joueur est
   avantage » — et REGLES §15 pose justement l'avantage au premier joueur
   comme LA question du palier 0. On mesure les deux separement.
```

Concrètement : les parties `2n` et `2n+1` partagent la **même graine** et
n'échangent que les sièges. C'est cet appariement qui isole l'avantage de
position.

D'où deux tuiles distinctes dans les résultats :

- **Camp 0 / Camp 1** : quelle *stratégie* gagne, sièges confondus.
- **Siège 1 gagne** : quel *siège* gagne, stratégies confondues. C'est
  l'avantage au premier joueur.

> **⚠️ L'inversion ne s'applique qu'à 2 joueurs**
>
> **`NkcBatch.h — SeatToConfig`**
>
> ```cpp
> /// Siege sur le plateau -> camp de la configuration. A 2 joueurs
> /// l'inversion echange 0 et 1 ; au-dela on ne l'applique pas (une
> /// permutation cyclique a 3-4 joueurs ne s'apparie plus deux a deux).
> ```
>
> À 3 ou 4 joueurs, les winrates par camp **mélangent** effet de stratégie et
> effet de siège. Tenez-en compte avant de conclure quoi que ce soit.

### Les tuiles rouges

**Coupées (max_tours)** doit valoir **zéro**. Dès qu'elle est rouge, la tuile
vous dit que des parties ne se terminent pas d'elles-mêmes.

**`REGLES_COMPLETES_v2.md:453-454`**

```
Un taux non nul signale une pathologie des règles, pas un réglage à monter.
```

**Coups illégaux** doit valoir zéro aussi. Un chiffre non nul veut dire qu'une IA
a proposé un coup que le moteur refuse — presque toujours un `memset` manquant
(chapitre 2.5), parfois une IA qui construit un coup au lieu de le choisir.

### Les deux histogrammes

- **Usage des actions** — cinq barres : *aucune*, *dupliquer*, *fusionner*,
  *pouvoir*, *passer*. La barre *aucune* doit rester à zéro : une valeur non nulle
  serait un bug du générateur de coups, pas un détail à masquer.

  Une action jamais jouée est une action à supprimer ou à rendre attractive.
  C'est ce raisonnement qui a tué l'action SAUTER :

  **`REGLES_COMPLETES_v2.md:57`**

  ```
  L'action SAUTER n'est jamais jouée | +0 totem, une seule cible, conquête
  impossible : dominée par DUPLIQUER dans toute position | Supprimée du jeu
  de base. Revient comme pouvoir, où elle a un coût.
  ```

- **Durée des parties** — une barre par tranche de dix coups. Une distribution
  très étalée, ou un pic contre la dernière barre, signale des parties qui
  traînent.

## 6.7 Une méthode de mesure qui tient

Quatre règles, apprises à leurs dépens par d'autres.

**1. Une variable à la fois.** Changez `portee_duplication`, mesurez, notez.
Puis remettez-la et changez autre chose. Deux variables ensemble donnent un
résultat que rien ne permet d'attribuer.

**2. Assez de parties.** Sur 20 parties, un écart de 10 points n'est que du
bruit. Comptez 200 pour une tendance, 1000 pour une conclusion, 10 000 pour une
décision de conception.

**3. Notez la graine et les réglages avec le chiffre.** Un winrate sans son
contexte n'est pas une mesure, c'est une anecdote. La barre du plateau affiche la
graine en permanence pour cette raison.

**4. Le résultat qui ne bouge pas est un résultat.** Si doubler la portée ne
change pas le winrate, vous venez d'apprendre que ce paramètre n'est pas un
levier. C'est une information, pas un échec.

> **⚠️ Le réglage modifié en cours de partie**
>
> Le panneau *Règles* affiche un bandeau orange dès que vous touchez un
> paramètre : « Réglage modifié — relance la partie pour mesurer ». Il est là
> parce que les coups **déjà joués** ne sont pas rejoués avec la nouvelle règle.
> Une partie à cheval sur deux réglages ne mesure rien.

## 6.8 Quand ça ne marche pas

| Symptôme | Regardez d'abord |
|---|---|
| rien ne bouge | **la barre d'état du plateau** — elle dit ce qu'on attend |
| mon module n'apparaît pas | panneau *Modules* : détecté ? compilé ? le chemin est-il le bon ? |
| « Symbole introuvable » | les deux `NKC_MODULE_EXPORT` en fin de fichier (chapitre 2.9) |
| « ABI 1, attendue 2 » | vous compilez contre de vieux en-têtes |
| « l'IA a proposé un coup ILLÉGAL » | le `memset` de votre coup (chapitre 2.5) |
| l'interface se fige une seconde | c'est une compilation en cours, pas un plantage |
| « le moteur a REFUSÉ ce plateau » | `cells` vide, JSON malformé, ou moteur sans `LoadBoardJson` |
| la campagne refuse de démarrer | chaque siège doit être tenu par une IA **chargée** |

## 6.9 Le banc d'essai, hors interface

Avant de croire à quoi que ce soit, faites tourner le banc d'essai. Il vérifie le
contrat de bout en bout, sans interface :

**`Applications/ConquerorLab/README.md — section « Reproduire »`**

```sh
CLANG="C:/msys64/ucrt64/bin/clang++.exe"
R="d:/Projets/2026/Nkentseu/Nkentseu"
INC="-I$R/Applications/ConquerorLab/include -I$R/Kernel/Foundation/NKCore/src -I$R/Kernel/Foundation/NKPlatform/src"

$CLANG -shared -std=c++17 -O2 -fPIC $INC -o ConquerorRulesV2.dll $R/Applications/ConquerorLab/modules/rules/ConquerorRulesV2.cpp
$CLANG -shared -std=c++17 -O2 -fPIC $INC -o ConquerorAIRef.dll   $R/Applications/ConquerorLab/modules/ai/ConquerorAIRef.cpp
$CLANG -std=c++17 -O1 $INC -o abitest.exe $R/Applications/ConquerorLab/tests/NkcAbiHarness.cpp
./abitest.exe
```

Seize vérifications, dont les trois qui comptent : l'IA ne produit jamais de coup
illégal, la partie se termine d'elle-même, et le rejeu redonne exactement la même
empreinte.

Remplacez les deux chemins de modules par les vôtres : le banc d'essai teste
**votre** module aussi bien que celui de référence.

## Exercices

> **✏️ 1 — Le premier chiffre**
>
> Lancez 200 parties, IA de référence *Normal* contre *Facile*, sur le plateau
> par défaut. Notez : winrate par camp, victoires du siège 1, taux de coupées,
> durée moyenne. Recommencez avec une autre graine. Les chiffres bougent-ils ?
> De combien ? Vous venez de mesurer votre bruit de fond — et donc le seuil en
> dessous duquel un écart ne veut rien dire.

> **✏️ 2 — L'avantage au premier joueur**
>
> Même IA des deux côtés, 1000 parties. Que vaut « Siège 1 gagne » ? Est-ce
> significatif au regard du bruit mesuré à l'exercice 1 ? C'est **la** question
> du palier 0 : écrivez votre réponse en une phrase, avec le chiffre.

> **✏️ 3 — Le paramètre qui ne sert à rien**
>
> Prenez un paramètre du moteur de référence et faites-le varier sur trois
> valeurs, 300 parties chacune. Tracez le winrate. Est-ce un levier ou une
> décoration ? Sur quel critère tranchez-vous ?

> **✏️ 4 — Reproduire une divergence**
>
> Introduisez volontairement du non-déterminisme dans votre moteur : par exemple,
> faites dépendre l'ordre des coups de l'adresse d'un pointeur. Jouez une partie,
> copiez le journal, relancez avec la même graine, comparez les empreintes.
> À quel coup la divergence apparaît-elle ? Retirez ensuite le défaut.
