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
