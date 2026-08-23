# Écrire des règles en trois fonctions

Un moteur de règles complet tient en trois méthodes :

| Méthode | La question à laquelle elle répond |
|---|---|
| `Construire` | à quoi la partie ressemble au départ |
| `CoupsPossibles` | ce qu'on a le droit de faire |
| `Appliquer` | ce qui se passe quand on le fait |

Tout le reste — créer et détruire une instance, cloner un état, le sérialiser,
le hacher, dire si la partie est finie, dire qui a gagné, vérifier qu'un coup
est légal — est **le même pour tout le monde**, et il est déjà écrit.

Le fichier de ce chapitre existe, il compile, et l'atelier le charge :
**`Applications/ConquerorLab/exemples/rules/RegleFacile.cpp`**.

## 2.1 Le compte, mesuré

`NkcRulesVTable` compte **24 entrées**. Voici où elles vont :

```
24  entrees de la vtable
 3  contiennent VOTRE jeu        Construire, CoupsPossibles, Appliquer
18  sont ecrites une fois pour toutes dans ConquerorRegleFacile.h
 3  sont optionnelles (geometrie) et restent nulles
```

Les recopier à chaque module, ce serait dix-huit occasions de se tromper sur du
code qui n'a aucun rapport avec le jeu qu'on essaie de concevoir.

## 2.2 Le module entier

Voici un moteur qui joue vraiment. Rien n'a été coupé.

```cpp
#include "Conqueror/ConquerorRegleFacile.h"

using namespace nkentseu;
using namespace nkentseu::conqueror;
using namespace nkentseu::conqueror::facile;

struct MesRegles {

    static constexpr int32 kCote = 5;

    // 1. A quoi la partie ressemble au depart.
    void Construire(Grille &g) {
        g.topologie = NkcTopology::Square4;   // 4 voisins : le plus simple a suivre
        g.nbJoueurs = 2;
        g.AjouterRectangle(kCote, kCote);
        g.PoserAuxCoins(2);                   // un totem par joueur, aux coins opposes
    }

    // 2. Ce qu'on a le droit de faire : dupliquer vers une case voisine VIDE.
    void CoupsPossibles(const Partie &p, ListeCoups &out) {
        NkcCoord voisins[8];
        for (int32 i = 0; i < p.nbCases; ++i) {
            if (p.cases[i].owner != static_cast<int8>(p.joueur)) continue;
            const NkcCoord de = p.ou[i];
            const int32    n  = p.Voisins(de, voisins, 8);
            for (int32 k = 0; k < n; ++k)
                if (p.Vide(voisins[k]))
                    out.Dupliquer(p.joueur, de, voisins[k]);
        }
        // PASSER est ajoute tout seul si cette liste reste vide.
    }

    // 3. Ce qui se passe quand on le fait.
    void Appliquer(Partie &p, const NkcMove &m, Evenements &ev) {
        p.Poser(m.to, m.player);
        ev.Duplique(m.player, m.from, m.to);
        p.conquete[m.player] += 10;           // en DIXIEMES entiers : 10 == 1,0 point

        NkcCoord    voisins[8];
        const int32 n = p.Voisins(m.to, voisins, 8);
        for (int32 k = 0; k < n; ++k) {
            if (!p.Ennemie(voisins[k], m.player)) continue;
            const int8 ancien = p.Proprietaire(voisins[k]);
            p.Poser(voisins[k], m.player);
            ev.Retourne(m.player, voisins[k], ancien);
        }
        p.PasserLaMain();
    }
};

NKC_REGLES(MesRegles, "MesRegles", "1.0.0", "Moi")
```

La dernière ligne fabrique les vingt et une entrées de la vtable et les deux
symboles exportés. **C'est tout le module.**

## 2.3 L'essayer

1. copiez `exemples/rules/RegleFacile.cpp` vers `travail/rules/mes_regles.cpp` ;
2. changez le nom dans `NKC_REGLES` — sinon deux entrées identiques au menu ;
3. sauvegardez, attendez une seconde ;
4. panneau **Modules** → votre module apparaît → **Nouvelle partie**.

Si la compilation échoue, la sortie complète du compilateur s'affiche dans le
panneau **Modules**. C'est votre seul retour : lisez-la.

## 2.4 Ce que l'échafaudage vous coûte

Il n'y a pas de magie, et la contrepartie se dit en une phrase : **`Partie` est
une structure fixe, plate, sans pointeur.**

C'est ce qui permet aux dix-huit fonctions d'être justes **par construction** et
non par relecture :

| ce que le cadre doit faire | comment il le fait, parce que `Partie` est plate |
|---|---|
| cloner un état | une affectation |
| sérialiser | un `memcpy` |
| hacher | FNV-1a sur les octets |
| dire si un coup est légal | énumérer les coups possibles et comparer |

Le prix : vous ne pouvez pas ranger dans `Partie` un état de taille libre — une
liste qui grandit, un cache, un arbre. Tant que votre jeu tient dans les cases,
les joueurs, l'énergie et la conquête, vous n'y penserez jamais.

## 2.4bis Le palier 1 : FUSIONNER — et comment ça se joue à l'écran

C'est la question qui bloque le plus de monde, alors on y répond dans l'ordre :
**d'abord le geste**, ensuite le code.

### Le geste, à la souris

Exactement le même que DUPLIQUER : **deux clics.**

1. vous cliquez **une des cases à consommer** → elle prend le contour orange
   « sélection », et des **anneaux verts** apparaissent sur les destinations ;
2. vous cliquez **l'anneau vert** → le coup part ;
3. le totem résultat apparaît un niveau plus haut, le journal affiche
   **`FUSIONNER`**, et l'atelier joue l'animation de fusion.

Vous **ne sélectionnez pas** les cases une par une. Votre module a déjà énuméré
les groupes possibles dans `CoupsPossibles` ; l'interface ne fait que
**retrouver** le coup que vous désignez par ces deux clics.

> ⚠️ **Ceci a été réparé le 2026-08-23, et il faut savoir pourquoi.** Un coup
> DUPLIQUER porte sa source dans `from`. Un coup FUSIONNER laisse `from` **à
> zéro** et met les cases consommées dans `fuseCells[0..fuseCount-1]`. Or
> l'atelier cherchait la case cliquée dans `from`, et seulement là : **toutes**
> les fusions lui semblaient partir de la case `(0,0)`. Conséquence mesurée : une
> IA jouait vos fusions sans broncher, le journal les affichait, et **vous ne
> pouviez pas les cliquer**. Si vous avez un kit antérieur au 23/08, c'est ce que
> vous constatiez — le défaut était dans l'atelier, pas dans vos règles.

### Le code : une trentaine de lignes de plus

**`Applications/ConquerorLab/exemples/rules/RegleFusion.cpp`** est
`RegleFacile.cpp` avec cette seule action en plus. Copiez-le dans
`travail/rules/` et jouez-le : c'est plus court à lire que ce chapitre.

Générer le coup :

```cpp
const NkcCoord paire[2] = {a, voisin};
out.Fusionner(p.joueur, paire, 2, a, niveau + 1);
//            joueur    cases  nb  vers  niveau visé
```

`Fusionner` existe pour **la même raison que `Dupliquer`** : le contrat compare
les coups **octet par octet**, un coup de fusion a deux champs de plus
(`fuseCount`, `fuseCells`), et les cases inutilisées du tableau doivent être à
zéro. Sinon le coup que votre IA propose n'est **pas égal** au coup que
`CoupsPossibles` avait généré, l'atelier le refuse, et rien dans votre logique
n'est faux. Le `memset` est dans le raccourci.

L'appliquer :

```cpp
if (m.kind == NkcMoveKind::Fuse) {
    for (int32 k = 0; k < m.fuseCount; ++k) p.Vider(m.fuseCells[k]);  // VIDER d'abord
    const int8 niveau = (m.targetLevel >= 0) ? m.targetLevel : 1;
    p.Poser(m.to, m.player, niveau);                                  // POSER ensuite
    ev.Fusionne(m.player, m.to, niveau);
    p.conquete[m.player] += 10 * niveau;
    p.PasserLaMain();
    return;
}
```

**Trois pièges, dans l'ordre où ils vous tomberont dessus.**

1. **Vider avant de poser.** `m.to` fait partie du groupe consommé. Poser avant
   de vider efface le totem qu'on vient de créer — et ça ne se voit qu'à la
   dixième partie, quand un totem disparaît sans raison.
2. **Ne proposer chaque paire qu'une fois.** Sans le filtre `Index(voisin) > i`,
   la paire {A,B} sort deux fois — vue de A, puis vue de B. Rien ne plante : la
   distribution des coups est seulement fausse, et votre campagne mesure autre
   chose que ce que vous croyez.
3. **Payer la fusion en conquête.** Un totem de niveau *n* vaut *n+1* totems. Si
   la conquête ne le reflète pas, fusionner fait **perdre** des points, aucune IA
   ne le fera jamais, et vous chercherez le bug dans votre générateur de coups.

### Ce qu'on en mesure

Deux campagnes, **même graine**, fusion activée puis désactivée. Si la durée des
parties ne bouge pas, votre fusion ne sert à rien : le problème est dans le
**coût** que vous lui donnez, pas dans le code. C'est ce genre de résultat qu'on
défend dans la fiche de travail.

> **Comment se joue une fusion à la souris ?** Ce n'est pas dans ce chapitre :
> il est écrit du point de vue de celui qui ÉCRIT le moteur. Le geste, les
> marques à l'ecran et le menu « Quel coup ? » sont au chapitre **6bis,
> [« Jouer un coup à la souris »](06b-jouer-un-coup.md)**.

Mesure du 2026-08-23 sur `RegleFusion.cpp` (carré 5×5, 2 joueurs, en prenant
systématiquement la fusion quand elle existe) : **196 coups joués dont 87
fusions, niveau 3 atteint, tous les coups légaux, rejeu déterministe.**

## 2.4ter Artefacts et pouvoirs : ce qui existe, et ce qui n'existe pas

Vous voulez anticiper — bien. Voici l'état **exact**, pour ne pas construire sur
du vide.

| | Dans le contrat | Dans l'atelier | Dans un module livré |
|---|---|---|---|
| **FUSIONNER** | oui — `NkcMoveKind::Fuse`, `fuseCells`, `fuseCount`, `targetLevel` | oui — jouable à la souris depuis le 23/08, journal `FUSIONNER` | **oui** — `exemples/rules/RegleFusion.cpp` |
| **POUVOIR** | oui — `NkcMoveKind::Power`, `powerId` ; `NkcCellView::power` et `powerUsed` | **oui depuis le 23/08** — clic sur le lanceur, clic sur la cible ; étiquette `P<id>`, et menu « Quel coup ? » quand plusieurs pouvoirs visent la même case | non |
| **ARTEFACT** | oui — `NkcCellView::artefact`, événements `ArtefactPlaced` / `ArtefactExpired` | **non** — rien ne les dessine | non |

Autrement dit : **les trois sont prévus dans le contrat, deux sont jouables à la
souris, aucun module livré ne pose encore d'artefact.**

- Un **pouvoir** est un coup `Power` avec `from` = le lanceur, `to` = la cible,
  `powerId` = lequel. Le geste est donc le même que pour dupliquer : **clic sur
  le lanceur, clic sur la cible**. Le troisième champ, `powerId`, est celui que
  deux clics ne peuvent pas dire — c'est pourquoi l'atelier ouvre le menu
  **« Quel coup ? »** dès que deux coups partagent la même destination. Avant le
  23/08 il jouait « le premier coup qui correspond » : le second pouvoir d'un
  totem sur une même cible était **injouable à la souris**, sans le moindre
  message. Le raccourci `Pouvoir(joueur, de, vers, idPouvoir)` construit le coup
  avec la mise à zéro qu'exige le contrat. Mesurez tout de même vos pouvoirs
  **en campagne IA contre IA** : c'est là que se tranche l'équilibrage.
- Un **artefact** est un état **de case**, pas un coup : `artefact` porte son id,
  et vous émettez `ArtefactPlaced` / `ArtefactExpired` quand il apparaît et
  disparaît. Le champ est **sérialisé** — un rejeu reste donc valide — mais rien
  ne l'affiche encore. `ConquerorRulesV2.cpp` le remet à `-1` et s'arrête là :
  c'est du palier 2.
- `NkcCellView::powerUsed` existe pour la **réserve** : un pouvoir attaché à un
  totem, consommable une fois. Le contrat le transporte ; personne ne s'en sert
  encore.

> **La règle, si vous vous lancez :** ne codez pas contre un champ que rien ne
> lit. Écrivez le pouvoir, faites-le jouer par l'IA, mesurez-le en campagne, et
> **dites dans votre fiche que vous n'avez pas pu le vérifier à la main** — c'est
> une réponse recevable, l'invention ne l'est pas.

## 2.5 Quand partir — et ce n'est pas un échec

> **C'est un échafaudage, pas une cage.**

Le jour où votre état ne rentre plus dans `Partie`, vous écrivez le contrat nu.
Les vingt-quatre entrées sont documentées au chapitre **« Quand l'échafaudage ne
suffit plus »**, à la fin de ce cours — et il se lira beaucoup mieux à ce
moment-là, parce que vous aurez déjà vu les dix-huit fonctions à l'œuvre.

Vous n'avez rien à défaire pour passer de l'un à l'autre : les deux produisent
le même module, avec les mêmes deux symboles. `RegleFacile.cpp` et
`RegleContratNu.cpp` jouent **exactement le même jeu** — comparez-les, c'est
l'exercice le plus instructif du cours.

## Exercices

1. **Portée 2.** Autorisez la duplication vers une case à deux pas. Une seule
   ligne change dans `CoupsPossibles` — laquelle ?
2. **Le déplacement.** Ajoutez l'action DEPLACER (la case de départ se vide).
   Que devient le décompte des totems ?
3. **Hexagone.** Passez `g.topologie` à `NkcTopology::HexPointy`. Combien de
   voisins avez-vous maintenant, et quelle ligne de votre code l'a supposé ?
4. **La cascade.** Émettez `ev.Cascade(...)` quand un coup retourne au moins
   deux ennemis, et regardez ce que l'atelier en fait à l'écran.
